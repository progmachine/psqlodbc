/*-------
 * Module:			bind.c
 *
 * Description:		This module contains routines related to binding
 *					columns and parameters.
 *
 * Classes:			BindInfoClass, ParameterInfoClass
 *
 * API functions:	SQLBindParameter, SQLBindCol, SQLDescribeParam, SQLNumParams,
 *					SQLParamOptions
 *
 * Comments:		See "notice.txt" for copyright and license information.
 *-------
 */

#include "bind.h"

#include "environ.h"
#include "statement.h"
#include "descriptor.h"
#include "qresult.h"
#include "pgtypes.h"
#include <stdlib.h>
#include <string.h>

#include "pgapifunc.h"


/*		Bind parameters on a statement handle */
RETCODE		SQL_API
PGAPI_BindParameter(
					HSTMT hstmt,
					UWORD ipar,
					SWORD fParamType,
					SWORD fCType,
					SWORD fSqlType,
					UDWORD cbColDef,
					SWORD ibScale,
					PTR rgbValue,
					SDWORD cbValueMax,
					SDWORD FAR * pcbValue)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	static char *func = "PGAPI_BindParameter";
	APDFields	*apdopts;
	IPDFields	*ipdopts;

	mylog("%s: entering...\n", func);

	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}
	SC_clear_error(stmt);

	apdopts = SC_get_APD(stmt);
	if (apdopts->allocated < ipar)
		extend_parameter_bindings(apdopts, ipar);
	ipdopts = SC_get_IPD(stmt);
	if (ipdopts->allocated < ipar)
		extend_iparameter_bindings(ipdopts, ipar);

	/* use zero based column numbers for the below part */
	ipar--;

	/* store the given info */
	apdopts->parameters[ipar].buflen = cbValueMax;
	apdopts->parameters[ipar].buffer = rgbValue;
	apdopts->parameters[ipar].used = pcbValue;
	apdopts->parameters[ipar].paramType = fParamType;
	apdopts->parameters[ipar].CType = fCType;
	ipdopts->parameters[ipar].SQLType = fSqlType;
	ipdopts->parameters[ipar].paramType = fParamType;
	ipdopts->parameters[ipar].column_size = cbColDef;
	ipdopts->parameters[ipar].decimal_digits = ibScale;
	ipdopts->parameters[ipar].precision = 0;
	ipdopts->parameters[ipar].scale = 0;
	ipdopts->parameters[ipar].PGType = sqltype_to_pgtype(stmt, fSqlType);
#if (ODBCVER >= 0x0300)
	switch (fCType)
	{
		case SQL_C_NUMERIC:
			if (cbColDef > 0)
				ipdopts->parameters[ipar].precision = (UInt2) cbColDef;
			if (ibScale > 0)
				ipdopts->parameters[ipar].scale = ibScale;
			break;
		case SQL_C_TYPE_TIMESTAMP:
			if (ibScale > 0)
				ipdopts->parameters[ipar].precision = ibScale;
			break;
	}
	apdopts->parameters[ipar].precision = ipdopts->parameters[ipar].precision;
	apdopts->parameters[ipar].scale = ipdopts->parameters[ipar].scale;
#endif /* ODBCVER */

	/*
	 * If rebinding a parameter that had data-at-exec stuff in it, then
	 * free that stuff
	 */
	if (apdopts->parameters[ipar].EXEC_used)
	{
		free(apdopts->parameters[ipar].EXEC_used);
		apdopts->parameters[ipar].EXEC_used = NULL;
	}

	if (apdopts->parameters[ipar].EXEC_buffer)
	{
		free(apdopts->parameters[ipar].EXEC_buffer);
		apdopts->parameters[ipar].EXEC_buffer = NULL;
	}

	if (pcbValue && apdopts->param_offset_ptr)
		pcbValue += (*apdopts->param_offset_ptr >> 2);
	/* Data at exec macro only valid for C char/binary data */
	if (pcbValue && (*pcbValue == SQL_DATA_AT_EXEC ||
					 *pcbValue <= SQL_LEN_DATA_AT_EXEC_OFFSET))
		apdopts->parameters[ipar].data_at_exec = TRUE;
	else
		apdopts->parameters[ipar].data_at_exec = FALSE;

	/* Clear premature result */
	if (stmt->status == STMT_PREMATURE)
		SC_recycle_statement(stmt);

	mylog("PGAPI_BindParamater: ipar=%d, paramType=%d, fCType=%d, fSqlType=%d, cbColDef=%d, ibScale=%d, rgbValue=%d, *pcbValue = %d, data_at_exec = %d\n", ipar, fParamType, fCType, fSqlType, cbColDef, ibScale, rgbValue, pcbValue ? *pcbValue : -777, apdopts->parameters[ipar].data_at_exec);

	return SQL_SUCCESS;
}


/*	Associate a user-supplied buffer with a database column. */
RETCODE		SQL_API
PGAPI_BindCol(
			  HSTMT hstmt,
			  UWORD icol,
			  SWORD fCType,
			  PTR rgbValue,
			  SDWORD cbValueMax,
			  SDWORD FAR * pcbValue)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	static char *func = "PGAPI_BindCol";
	ARDFields	*opts;

	mylog("%s: entering...\n", func);

	mylog("**** PGAPI_BindCol: stmt = %u, icol = %d\n", stmt, icol);
	mylog("**** : fCType=%d rgb=%x valusMax=%d pcb=%x\n", fCType, rgbValue, cbValueMax, pcbValue);

	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}



	opts = SC_get_ARD(stmt);
	if (stmt->status == STMT_EXECUTING)
	{
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "Can't bind columns while statement is still executing.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}

	SC_clear_error(stmt);
	/* If the bookmark column is being bound, then just save it */
	if (icol == 0)
	{
		if (rgbValue == NULL)
		{
			opts->bookmark->buffer = NULL;
			opts->bookmark->used = NULL;
		}
		else
		{
			/* Make sure it is the bookmark data type */
			switch (fCType)
			{
				case SQL_C_BOOKMARK:
#if (ODBCVER >= 0x0300)
				case SQL_C_VARBOOKMARK:
#endif /* ODBCVER */
					break;
				default:
					SC_set_error(stmt, STMT_PROGRAM_TYPE_OUT_OF_RANGE, "Column 0 is not of type SQL_C_BOOKMARK");
inolog("Column 0 is type %d not of type SQL_C_BOOKMARK", fCType);
					SC_log_error(func, "", stmt);
					return SQL_ERROR;
			}

			opts->bookmark->buffer = rgbValue;
			opts->bookmark->used = pcbValue;
		}
		return SQL_SUCCESS;
	}

	/*
	 * Allocate enough bindings if not already done. Most likely,
	 * execution of a statement would have setup the necessary bindings.
	 * But some apps call BindCol before any statement is executed.
	 */
	if (icol > opts->allocated)
		extend_column_bindings(opts, icol);

	/* check to see if the bindings were allocated */
	if (!opts->bindings)
	{
		SC_set_error(stmt, STMT_NO_MEMORY_ERROR, "Could not allocate memory for bindings.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}

	/* use zero based col numbers from here out */
	icol--;

	/* Reset for SQLGetData */
	opts->bindings[icol].data_left = -1;

	if (rgbValue == NULL)
	{
		/* we have to unbind the column */
		opts->bindings[icol].buflen = 0;
		opts->bindings[icol].buffer = NULL;
		opts->bindings[icol].used = NULL;
		opts->bindings[icol].returntype = SQL_C_CHAR;
		if (opts->bindings[icol].ttlbuf)
			free(opts->bindings[icol].ttlbuf);
		opts->bindings[icol].ttlbuf = NULL;
		opts->bindings[icol].ttlbuflen = 0;
		opts->bindings[icol].ttlbufused = 0;
		opts->bindings[icol].precision = 0;
		opts->bindings[icol].scale = 0;
	}
	else
	{
		/* ok, bind that column */
		opts->bindings[icol].buflen = cbValueMax;
		opts->bindings[icol].buffer = rgbValue;
		opts->bindings[icol].used = pcbValue;
		opts->bindings[icol].returntype = fCType;
#if (ODBCVER >= 0x0300)
		if (SQL_C_NUMERIC == fCType)
			opts->bindings[icol].precision = 32;
		else
#endif /* ODBCVER */
			opts->bindings[icol].precision = 0;
		opts->bindings[icol].scale = 0;

		mylog("       bound buffer[%d] = %u\n", icol, opts->bindings[icol].buffer);
	}

	return SQL_SUCCESS;
}


/*
 *	Returns the description of a parameter marker.
 *	This function is listed as not being supported by SQLGetFunctions() because it is
 *	used to describe "parameter markers" (not bound parameters), in which case,
 *	the dbms should return info on the markers.  Since Postgres doesn't support that,
 *	it is best to say this function is not supported and let the application assume a
 *	data type (most likely varchar).
 */
RETCODE		SQL_API
PGAPI_DescribeParam(
					HSTMT hstmt,
					UWORD ipar,
					SWORD FAR * pfSqlType,
					UDWORD FAR * pcbColDef,
					SWORD FAR * pibScale,
					SWORD FAR * pfNullable)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	static char *func = "PGAPI_DescribeParam";
	APDFields	*apdopts;
	IPDFields	*ipdopts;

	mylog("%s: entering...\n", func);

	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}
	SC_clear_error(stmt);

	apdopts = SC_get_APD(stmt);
	if ((ipar < 1) || (ipar > apdopts->allocated))
	{
		SC_set_error(stmt, STMT_BAD_PARAMETER_NUMBER_ERROR, "Invalid parameter number for PGAPI_DescribeParam.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	ipdopts = SC_get_IPD(stmt);

	ipar--;

	/*
	 * This implementation is not very good, since it is supposed to
	 * describe
	 */
	/* parameter markers, not bound parameters.  */
	if (pfSqlType)
		*pfSqlType = ipdopts->parameters[ipar].SQLType;

	if (pcbColDef)
		*pcbColDef = ipdopts->parameters[ipar].column_size;

	if (pibScale)
		*pibScale = ipdopts->parameters[ipar].decimal_digits;

	if (pfNullable)
		*pfNullable = pgtype_nullable(stmt, apdopts->parameters[ipar].paramType);

	return SQL_SUCCESS;
}


/*	Sets multiple values (arrays) for the set of parameter markers. */
RETCODE		SQL_API
PGAPI_ParamOptions(
				   HSTMT hstmt,
				   UDWORD crow,
				   UDWORD FAR * pirow)
{
	static char *func = "PGAPI_ParamOptions";
	StatementClass *stmt = (StatementClass *) hstmt;
	APDFields	*apdopts;

	mylog("%s: entering... %d %x\n", func, crow, pirow);

	apdopts = SC_get_APD(stmt);
	apdopts->paramset_size = crow;
	SC_get_IPD(stmt)->param_processed_ptr = (UInt4 *) pirow;
	return SQL_SUCCESS;
}


/*
 *	This function should really talk to the dbms to determine the number of
 *	"parameter markers" (not bound parameters) in the statement.  But, since
 *	Postgres doesn't support that, the driver should just count the number of markers
 *	and return that.  The reason the driver just can't say this function is unsupported
 *	like it does for SQLDescribeParam is that some applications don't care and try
 *	to call it anyway.
 *	If the statement does not have parameters, it should just return 0.
 */
RETCODE		SQL_API
PGAPI_NumParams(
				HSTMT hstmt,
				SWORD FAR * pcpar)
{
	StatementClass *stmt = (StatementClass *) hstmt;
	char		in_quote = FALSE;
	unsigned int i;
	static char *func = "PGAPI_NumParams";

	mylog("%s: entering...\n", func);

	if (!stmt)
	{
		SC_log_error(func, "", NULL);
		return SQL_INVALID_HANDLE;
	}

	if (pcpar)
		*pcpar = 0;
	else
	{
		SC_log_error(func, "pcpar was null", stmt);
		return SQL_ERROR;
	}


	if (!stmt->statement)
	{
		/* no statement has been allocated */
		SC_set_error(stmt, STMT_SEQUENCE_ERROR, "PGAPI_NumParams called with no statement ready.");
		SC_log_error(func, "", stmt);
		return SQL_ERROR;
	}
	else
	{
		for (i = 0; i < strlen(stmt->statement); i++)
		{
			if (stmt->statement[i] == '?' && !in_quote)
				(*pcpar)++;
			else
			{
				if (stmt->statement[i] == '\'')
					in_quote = (in_quote ? FALSE : TRUE);
			}
		}
		return SQL_SUCCESS;
	}
}


/*
 *	 Bindings Implementation
 */
BindInfoClass *
create_empty_bindings(int num_columns)
{
	BindInfoClass *new_bindings;
	int			i;

	new_bindings = (BindInfoClass *) malloc(num_columns * sizeof(BindInfoClass));
	if (!new_bindings)
		return 0;

	for (i = 0; i < num_columns; i++)
	{
		new_bindings[i].buflen = 0;
		new_bindings[i].buffer = NULL;
		new_bindings[i].used = NULL;
		new_bindings[i].data_left = -1;
		new_bindings[i].ttlbuf = NULL;
		new_bindings[i].ttlbuflen = 0;
		new_bindings[i].ttlbufused = 0;
	}

	return new_bindings;
}

void
extend_parameter_bindings(APDFields *self, int num_params)
{
	static char *func = "extend_parameter_bindings";
	ParameterInfoClass *new_bindings;

	mylog("%s: entering ... self=%u, parameters_allocated=%d, num_params=%d\n", func, self, self->allocated, num_params);

	/*
	 * if we have too few, allocate room for more, and copy the old
	 * entries into the new structure
	 */
	if (self->allocated < num_params)
	{
		new_bindings = (ParameterInfoClass *) realloc(self->parameters, sizeof(ParameterInfoClass) * num_params);
		if (!new_bindings)
		{
			mylog("%s: unable to create %d new bindings from %d old bindings\n", func, num_params, self->allocated);

			self->parameters = NULL;
			self->allocated = 0;
			return;
		}
		memset(&new_bindings[self->allocated], 0,
			sizeof(ParameterInfoClass) * (num_params - self->allocated));

		self->parameters = new_bindings;
		self->allocated = num_params;
	}

	mylog("exit extend_parameter_bindings\n");
}

void
extend_iparameter_bindings(IPDFields *self, int num_params)
{
	static char *func = "extend_iparameter_bindings";
	ParameterImplClass *new_bindings;

	mylog("%s: entering ... self=%u, parameters_allocated=%d, num_params=%d\n", func, self, self->allocated, num_params);

	/*
	 * if we have too few, allocate room for more, and copy the old
	 * entries into the new structure
	 */
	if (self->allocated < num_params)
	{
		new_bindings = (ParameterImplClass *) realloc(self->parameters, sizeof(ParameterImplClass) * num_params);
		if (!new_bindings)
		{
			mylog("%s: unable to create %d new bindings from %d old bindings\n", func, num_params, self->allocated);

			self->parameters = NULL;
			self->allocated = 0;
			return;
		}
		memset(&new_bindings[self->allocated], 0,
			sizeof(ParameterImplClass) * (num_params - self->allocated));

		self->parameters = new_bindings;
		self->allocated = num_params;
	}

	mylog("exit extend_iparameter_bindings\n");
}

void
reset_a_parameter_binding(APDFields *self, int ipar)
{
	static char *func = "reset_a_parameter_binding";

	mylog("%s: entering ... self=%u, parameters_allocated=%d, ipar=%d\n", func, self, self->allocated, ipar);

	if (ipar < 1 || ipar > self->allocated)
		return;

	ipar--;
	self->parameters[ipar].buflen = 0;
	self->parameters[ipar].buffer = 0;
	self->parameters[ipar].used = 0;
	self->parameters[ipar].paramType = 0;
	self->parameters[ipar].CType = 0;
	self->parameters[ipar].data_at_exec = FALSE;
	if (self->parameters[ipar].EXEC_used)
	{
		free(self->parameters[ipar].EXEC_used);
		self->parameters[ipar].EXEC_used = NULL;
	}

	if (self->parameters[ipar].EXEC_buffer)
	{
		free(self->parameters[ipar].EXEC_buffer);
		self->parameters[ipar].EXEC_buffer = NULL;
	}
	self->parameters[ipar].lobj_oid = 0;
	self->parameters[ipar].precision = 0;
	self->parameters[ipar].scale = 0;
}

void
reset_a_iparameter_binding(IPDFields *self, int ipar)
{
	static char *func = "reset_a_iparameter_binding";

	mylog("%s: entering ... self=%u, parameters_allocated=%d, ipar=%d\n", func, self, self->allocated, ipar);

	if (ipar < 1 || ipar > self->allocated)
		return;

	ipar--;
	self->parameters[ipar].paramType = 0;
	self->parameters[ipar].SQLType = 0;
	self->parameters[ipar].column_size = 0;
	self->parameters[ipar].decimal_digits = 0;
	self->parameters[ipar].precision = 0;
	self->parameters[ipar].scale = 0;
}

/*
 *	Free parameters and free the memory.
 */
void
APD_free_params(APDFields *apdopts, char option)
{
	int			i;

	mylog("APD_free_params:  ENTER, self=%d\n", apdopts);

	if (!apdopts->parameters)
		return;

	for (i = 0; i < apdopts->allocated; i++)
	{
		if (apdopts->parameters[i].data_at_exec)
		{
			if (apdopts->parameters[i].EXEC_used)
			{
				free(apdopts->parameters[i].EXEC_used);
				apdopts->parameters[i].EXEC_used = NULL;
			}
			if (apdopts->parameters[i].EXEC_buffer)
			{
				free(apdopts->parameters[i].EXEC_buffer);
				apdopts->parameters[i].EXEC_buffer = NULL;
			}
		}
	}

	if (option == STMT_FREE_PARAMS_ALL)
	{
		if (apdopts->parameters)
			free(apdopts->parameters);
		apdopts->parameters = NULL;
		apdopts->allocated = 0;
	}

	mylog("APD_free_params:  EXIT\n");
}

/*
 *	Free parameters and free the memory.
 */
void
IPD_free_params(IPDFields *ipdopts, char option)
{
	mylog("IPD_free_params:  ENTER, self=%d\n", ipdopts);

	if (ipdopts->parameters &&
		option == STMT_FREE_PARAMS_ALL)
	{
		if (ipdopts->parameters)
			free(ipdopts->parameters);
		ipdopts->parameters = NULL;
		ipdopts->allocated = 0;
	}

	mylog("IPD_free_params:  EXIT\n");
}

void
extend_column_bindings(ARDFields *self, int num_columns)
{
	static char *func = "extend_column_bindings";
	BindInfoClass *new_bindings;
	int			i;

	mylog("%s: entering ... self=%u, bindings_allocated=%d, num_columns=%d\n", func, self, self->allocated, num_columns);

	/*
	 * if we have too few, allocate room for more, and copy the old
	 * entries into the new structure
	 */
	if (self->allocated < num_columns)
	{
		new_bindings = create_empty_bindings(num_columns);
		if (!new_bindings)
		{
			mylog("%s: unable to create %d new bindings from %d old bindings\n", func, num_columns, self->allocated);

			if (self->bindings)
			{
				free(self->bindings);
				self->bindings = NULL;
			}
			self->allocated = 0;
			return;
		}

		if (self->bindings)
		{
			for (i = 0; i < self->allocated; i++)
				new_bindings[i] = self->bindings[i];

			free(self->bindings);
		}

		self->bindings = new_bindings;
		self->allocated = num_columns;
	}

	/*
	 * There is no reason to zero out extra bindings if there are more
	 * than needed.  If an app has allocated extra bindings, let it worry
	 * about it by unbinding those columns.
	 */

	/* SQLBindCol(1..) ... SQLBindCol(10...)   # got 10 bindings */
	/* SQLExecDirect(...)  # returns 5 cols */
	/* SQLExecDirect(...)  # returns 10 cols  (now OK) */

	mylog("exit extend_column_bindings\n");
}

void
reset_a_column_binding(ARDFields *self, int icol)
{
	static char *func = "reset_a_column_binding";

	mylog("%s: entering ... self=%u, bindings_allocated=%d, icol=%d\n", func, self, self->allocated, icol);

	if (icol > self->allocated)
		return;

	/* use zero based col numbers from here out */
	if (0 == icol)
	{
		self->bookmark->buffer = NULL;
		self->bookmark->used = NULL;
	}
	else
	{
		icol--;

		/* we have to unbind the column */
		self->bindings[icol].buflen = 0;
		self->bindings[icol].buffer = NULL;
		self->bindings[icol].used = NULL;
		self->bindings[icol].data_left = -1;
		self->bindings[icol].returntype = SQL_C_CHAR;
		if (self->bindings[icol].ttlbuf)
			free(self->bindings[icol].ttlbuf);
		self->bindings[icol].ttlbuf = NULL;
		self->bindings[icol].ttlbuflen = 0;
		self->bindings[icol].ttlbufused = 0;
	}
}

void	ARD_unbind_cols(ARDFields *self, BOOL freeall)
{
	Int2	lf;

	for (lf = 1; lf <= self->allocated; lf++)
		reset_a_column_binding(self, lf);
	if (freeall)
	{
		if (self->bindings)
			free(self->bindings);
		self->bindings = NULL;
		self->allocated = 0;
	}
} 
