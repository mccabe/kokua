/** 
 * @file lscript_execute.cpp
 * @brief classes to execute bytecode
 *
 * Copyright (c) 2002-$CurrentYear$, Linden Research, Inc.
 * $License$
 */

#include "linden_common.h"

#include <sstream>

#include "lscript_execute.h"
#include "lltimer.h"
#include "lscript_readlso.h"
#include "lscript_library.h"
#include "lscript_heapruntime.h"
#include "lscript_alloc.h"

void (*binary_operations[LST_EOF][LST_EOF])(U8 *buffer, LSCRIPTOpCodesEnum opcode);
void (*unary_operations[LST_EOF])(U8 *buffer, LSCRIPTOpCodesEnum opcode);

char *LSCRIPTRunTimeFaultStrings[LSRF_EOF] =
{
	"invalid",				//	LSRF_INVALID,
	"Math Error",			//	LSRF_MATH,
	"Stack-Heap Collision",	//	LSRF_STACK_HEAP_COLLISION,
	"Bounds Check Error",	//	LSRF_BOUND_CHECK_ERROR,
	"Heap Error",			//	LSRF_HEAP_ERROR,
	"Version Mismatch",		//	LSRF_VERSION_MISMATCH,
	"Missing Inventory",	//	LSRF_MISSING_INVENTORY,
	"Hit Sandbox Limit",	//	LSRF_SANDBOX,
	"Chat Overrun",			//	LSRF_CHAT_OVERRUN,
	"Too Many Listens",			  //	LSRF_TOO_MANY_LISTENS,
	"Lists may not contain lists" //	LSRF_NESTING_LISTS,
};

//static
S64 LLScriptExecute::sGlobalInstructionCount = 0;

LLScriptExecute::LLScriptExecute(FILE *fp)
{
	U8  sizearray[4];
	S32 filesize;
	S32 pos = 0;
	fread(&sizearray, 1, 4, fp);
	filesize = bytestream2integer(sizearray, pos);
	mBuffer = new U8[filesize];
	fseek(fp, 0, SEEK_SET);
	fread(mBuffer, 1, filesize, fp);
	fclose(fp);

	init();
}

LLScriptExecute::LLScriptExecute(U8 *buffer)
{
	mBuffer = buffer; 

	init();
}

LLScriptExecute::~LLScriptExecute()
{
	delete [] mBuffer;
}

void LLScriptExecute::init()
{
	S32 i, j;

	mInstructionCount = 0;

	for (i = 0; i < 256; i++)
	{
		mExecuteFuncs[i] = run_noop;
	}
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_NOOP]] = run_noop;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POP]] = run_pop;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPS]] = run_pops;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPL]] = run_popl;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPV]] = run_popv;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPQ]] = run_popq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPARG]] = run_poparg;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPIP]] = run_popip;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPBP]] = run_popbp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPSP]] = run_popsp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_POPSLR]] = run_popslr;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_DUP]] = run_dup;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_DUPS]] = run_dups;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_DUPL]] = run_dupl;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_DUPV]] = run_dupv;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_DUPQ]] = run_dupq;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STORE]] = run_store;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STORES]] = run_stores;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STOREL]] = run_storel;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STOREV]] = run_storev;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STOREQ]] = run_storeq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STOREG]] = run_storeg;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STOREGL]] = run_storegl;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STOREGS]] = run_storegs;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STOREGV]] = run_storegv;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STOREGQ]] = run_storegq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADP]] = run_loadp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADSP]] = run_loadsp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADLP]] = run_loadlp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADVP]] = run_loadvp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADQP]] = run_loadqp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADGP]] = run_loadgp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADGSP]] = run_loadgsp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADGLP]] = run_loadglp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADGVP]] = run_loadgvp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LOADGQP]] = run_loadgqp;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSH]] = run_push;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHS]] = run_pushs;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHL]] = run_pushl;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHV]] = run_pushv;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHQ]] = run_pushq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHG]] = run_pushg;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHGS]] = run_pushgs;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHGL]] = run_pushgl;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHGV]] = run_pushgv;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHGQ]] = run_pushgq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHIP]] = run_puship;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHSP]] = run_pushsp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHBP]] = run_pushbp;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHARGB]] = run_pushargb;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHARGI]] = run_pushargi;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHARGF]] = run_pushargf;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHARGS]] = run_pushargs;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHARGV]] = run_pushargv;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHARGQ]] = run_pushargq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHE]] = run_pushe;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHEV]] = run_pushev;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHEQ]] = run_pusheq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PUSHARGE]] = run_pusharge;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_ADD]] = run_add;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_SUB]] = run_sub;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_MUL]] = run_mul;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_DIV]] = run_div;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_MOD]] = run_mod;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_EQ]] = run_eq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_NEQ]] = run_neq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LEQ]] = run_leq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_GEQ]] = run_geq;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_LESS]] = run_less;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_GREATER]] = run_greater;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_BITAND]] = run_bitand;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_BITOR]] = run_bitor;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_BITXOR]] = run_bitxor;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_BOOLAND]] = run_booland;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_BOOLOR]] = run_boolor;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_SHL]] = run_shl;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_SHR]] = run_shr;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_NEG]] = run_neg;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_BITNOT]] = run_bitnot;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_BOOLNOT]] = run_boolnot;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_JUMP]] = run_jump;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_JUMPIF]] = run_jumpif;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_JUMPNIF]] = run_jumpnif;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STATE]] = run_state;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_CALL]] = run_call;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_RETURN]] = run_return;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_CAST]] = run_cast;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STACKTOS]] = run_stacktos;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_STACKTOL]] = run_stacktol;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_PRINT]] = run_print;

	mExecuteFuncs[LSCRIPTOpCodes[LOPC_CALLLIB]] = run_calllib;
	mExecuteFuncs[LSCRIPTOpCodes[LOPC_CALLLIB_TWO_BYTE]] = run_calllib_two_byte;

	for (i = 0; i < LST_EOF; i++)
	{
		for (j = 0; j < LST_EOF; j++)
		{
			binary_operations[i][j] = unknown_operation;
		}
	}

	binary_operations[LST_INTEGER][LST_INTEGER] = integer_integer_operation;
	binary_operations[LST_INTEGER][LST_FLOATINGPOINT] = integer_float_operation;
	binary_operations[LST_INTEGER][LST_VECTOR] = integer_vector_operation;

	binary_operations[LST_FLOATINGPOINT][LST_INTEGER] = float_integer_operation;
	binary_operations[LST_FLOATINGPOINT][LST_FLOATINGPOINT] = float_float_operation;
	binary_operations[LST_FLOATINGPOINT][LST_VECTOR] = float_vector_operation;

	binary_operations[LST_STRING][LST_STRING] = string_string_operation;
	binary_operations[LST_STRING][LST_KEY] = string_key_operation;

	binary_operations[LST_KEY][LST_STRING] = key_string_operation;
	binary_operations[LST_KEY][LST_KEY] = key_key_operation;

	binary_operations[LST_VECTOR][LST_INTEGER] = vector_integer_operation;
	binary_operations[LST_VECTOR][LST_FLOATINGPOINT] = vector_float_operation;
	binary_operations[LST_VECTOR][LST_VECTOR] = vector_vector_operation;
	binary_operations[LST_VECTOR][LST_QUATERNION] = vector_quaternion_operation;

	binary_operations[LST_QUATERNION][LST_QUATERNION] = quaternion_quaternion_operation;

	binary_operations[LST_INTEGER][LST_LIST] = integer_list_operation;
	binary_operations[LST_FLOATINGPOINT][LST_LIST] = float_list_operation;
	binary_operations[LST_STRING][LST_LIST] = string_list_operation;
	binary_operations[LST_KEY][LST_LIST] = key_list_operation;
	binary_operations[LST_VECTOR][LST_LIST] = vector_list_operation;
	binary_operations[LST_QUATERNION][LST_LIST] = quaternion_list_operation;
	binary_operations[LST_LIST][LST_INTEGER] = list_integer_operation;
	binary_operations[LST_LIST][LST_FLOATINGPOINT] = list_float_operation;
	binary_operations[LST_LIST][LST_STRING] = list_string_operation;
	binary_operations[LST_LIST][LST_KEY] = list_key_operation;
	binary_operations[LST_LIST][LST_VECTOR] = list_vector_operation;
	binary_operations[LST_LIST][LST_QUATERNION] = list_quaternion_operation;
	binary_operations[LST_LIST][LST_LIST] = list_list_operation;

	for (i = 0; i < LST_EOF; i++)
	{
		unary_operations[i] = unknown_operation;
	}

	unary_operations[LST_INTEGER] = integer_operation;
	unary_operations[LST_FLOATINGPOINT] = float_operation;
	unary_operations[LST_VECTOR] = vector_operation;
	unary_operations[LST_QUATERNION] = quaternion_operation;

}

S32 lscript_push_variable(LLScriptLibData *data, U8 *buffer);

U32 LLScriptExecute::run(BOOL b_print, const LLUUID &id, char **errorstr, BOOL &state_transition)
{
	//  is there a fault?
	//	if yes, print out message and exit
	state_transition = FALSE;
	S32 value = get_register(mBuffer, LREG_VN);
	S32 major_version = 0;
	if (value == LSL2_VERSION1_END_NUMBER)
	{
		major_version = 1;
	}
	else if (value == LSL2_VERSION_NUMBER)
	{
		major_version = 2;
	}
	else
	{
		set_fault(mBuffer, LSRF_VERSION_MISMATCH);
	}
	value = get_register(mBuffer, LREG_FR);
	if (value)
	{
		if (b_print)
		{
			printf("Error!\n");
		}
		*errorstr = LSCRIPTRunTimeFaultStrings[value];
		return NO_DELETE_FLAG;
	}
	else
	{
		*errorstr = NULL;
	}

	//  Get IP
	//  is IP nonzero?
	value = get_register(mBuffer, LREG_IP);

	if (value)
	{
	//	if yes, we're in opcodes, execute the next opcode by:
	//	call opcode run function pointer with buffer and IP
		mInstructionCount++;
		sGlobalInstructionCount++;
		S32 tvalue = value;
		S32	opcode = safe_instruction_bytestream2byte(mBuffer, tvalue);
		S32 b_ret_val = mExecuteFuncs[opcode](mBuffer, value, b_print, id);
		set_ip(mBuffer, value);
		add_register_fp(mBuffer, LREG_ESR, -0.1f);
	//	lsa_print_heap(mBuffer);

		if (b_print)
		{
			lsa_print_heap(mBuffer);
			printf("ip: 0x%X\n", get_register(mBuffer, LREG_IP));
			printf("sp: 0x%X\n", get_register(mBuffer, LREG_SP));
			printf("bp: 0x%X\n", get_register(mBuffer, LREG_BP));
			printf("hr: 0x%X\n", get_register(mBuffer, LREG_HR));
			printf("hp: 0x%X\n", get_register(mBuffer, LREG_HP));
		}
	//			update IP
		if (b_ret_val)
		{
			return DELETE_FLAG | CREDIT_MONEY_FLAG;
		}
		else
		{
			return NO_DELETE_FLAG;
		}
	}
	else
	{
		// make sure that IE is zero
		set_event_register(mBuffer, LREG_IE, 0, major_version);

	//	if no, we're in a state and waiting for an event
		S32 next_state = get_register(mBuffer, LREG_NS);
		S32 current_state = get_register(mBuffer, LREG_CS);
		U64 current_events = get_event_register(mBuffer, LREG_CE, major_version);
		U64 event_register = get_event_register(mBuffer, LREG_ER, major_version);
	//	check NS to see if need to switch states (NS != CS)
		if (next_state != current_state)
		{
			state_transition = TRUE;
			// ok, blow away any pending events
			mEventData.mEventDataList.deleteAllData();

	//		if yes, check state exit flag is set
			if (current_events & LSCRIPTStateBitField[LSTT_STATE_EXIT])
			{
	//			if yes, clear state exit flag
				set_event_register(mBuffer, LREG_IE, LSCRIPTStateBitField[LSTT_STATE_EXIT], major_version);
				current_events &= ~LSCRIPTStateBitField[LSTT_STATE_EXIT];
				set_event_register(mBuffer, LREG_CE, current_events, major_version);
	//			check state exit event handler
	//				if there is a handler, call it
				if (event_register & LSCRIPTStateBitField[LSTT_STATE_EXIT])
				{
		//			push a zero to be popped
					lscript_push(mBuffer, 0);
		//			push sp as current bp
					S32 sp = get_register(mBuffer, LREG_SP);
					lscript_push(mBuffer, sp);

		//			now, push any additional stack space
					S32 additional_size = get_event_stack_size(mBuffer, current_state, LSTT_STATE_EXIT);
					lscript_pusharge(mBuffer, additional_size);

					sp = get_register(mBuffer, LREG_SP);
					sp += additional_size;
					set_bp(mBuffer, sp);
	//				set IP to the event handler
					S32			opcode_start = get_state_event_opcoode_start(mBuffer, current_state, LSTT_STATE_EXIT);
					set_ip(mBuffer, opcode_start);
					return NO_DELETE_FLAG;
				}
			}
	//		if no handler or no state exit flag switch to new state
	//		set state entry flag and clear other CE flags
			current_events = LSCRIPTStateBitField[LSTT_STATE_ENTRY];
			set_event_register(mBuffer, LREG_CE, current_events, major_version);
	//		copy NS to CS
			set_register(mBuffer, LREG_CS, next_state);
	//		copy new state's handled events into ER (SR + CS*4 + 4)
			U64 handled_events = get_handled_events(mBuffer, next_state);
			set_event_register(mBuffer, LREG_ER, handled_events, major_version);
		}
//		check to see if any current events are covered by events handled by this state (CE & ER != 0)
// now, we want to look like we were called like a function
//		0x0000:		00 00 00 00 (return ip)
//		0x0004:		bp			(current sp)
//		0x0008:		parameters
//		push sp
//		add parameter size
//		pop bp
//		set ip

		S32 size = 0;
//			try to get next event from stack
		BOOL b_done = FALSE;
		LSCRIPTStateEventType event = LSTT_NULL;
		LLScriptDataCollection *eventdata;

		next_state = get_register(mBuffer, LREG_NS);
		current_state = get_register(mBuffer, LREG_CS);
		current_events = get_event_register(mBuffer, LREG_CE, major_version);
		event_register = get_event_register(mBuffer, LREG_ER, major_version);

		// first, check to see if state_entry or onrez are raised and handled
		if (  (current_events & LSCRIPTStateBitField[LSTT_STATE_ENTRY])
			&&(current_events & event_register))
		{
			// ok, this is easy since there isn't any data waiting, just set it
			//			push a zero to be popped
			lscript_push(mBuffer, 0);
//			push sp as current bp
			S32 sp = get_register(mBuffer, LREG_SP);
			lscript_push(mBuffer, sp);

			event = return_first_event((S32)LSCRIPTStateBitField[LSTT_STATE_ENTRY]);
			set_event_register(mBuffer, LREG_IE, LSCRIPTStateBitField[event], major_version);
			current_events &= ~LSCRIPTStateBitField[event];
			set_event_register(mBuffer, LREG_CE, current_events, major_version);
//			now, push any additional stack space
			S32 additional_size = get_event_stack_size(mBuffer, current_state, event) - size;
			lscript_pusharge(mBuffer, additional_size);

//			now set the bp correctly
			sp = get_register(mBuffer, LREG_SP);
			sp += additional_size + size;
			set_bp(mBuffer, sp);
//			set IP to the function
			S32			opcode_start = get_state_event_opcoode_start(mBuffer, current_state, event);
			set_ip(mBuffer, opcode_start);
			b_done = TRUE;
		}
		else if (  (current_events & LSCRIPTStateBitField[LSTT_REZ])
				 &&(current_events & event_register))
		{
			for (eventdata = mEventData.mEventDataList.getFirstData(); eventdata; eventdata = mEventData.mEventDataList.getNextData())
			{
				if (eventdata->mType & LSCRIPTStateBitField[LSTT_REZ])
				{
					//			push a zero to be popped
					lscript_push(mBuffer, 0);
		//			push sp as current bp
					S32 sp = get_register(mBuffer, LREG_SP);
					lscript_push(mBuffer, sp);

					set_event_register(mBuffer, LREG_IE, LSCRIPTStateBitField[event], major_version);
					current_events &= ~LSCRIPTStateBitField[event];
					set_event_register(mBuffer, LREG_CE, current_events, major_version);

	//				push any arguments that need to be pushed onto the stack
					// last piece of data will be type LST_NULL
					LLScriptLibData	*data = eventdata->mData;
					while (data->mType)
					{
						size += lscript_push_variable(data, mBuffer);
						data++;
					}
		//			now, push any additional stack space
					S32 additional_size = get_event_stack_size(mBuffer, current_state, event) - size;
					lscript_pusharge(mBuffer, additional_size);

		//			now set the bp correctly
					sp = get_register(mBuffer, LREG_SP);
					sp += additional_size + size;
					set_bp(mBuffer, sp);
		//			set IP to the function
					S32			opcode_start = get_state_event_opcoode_start(mBuffer, current_state, event);
					set_ip(mBuffer, opcode_start);
					mEventData.mEventDataList.deleteCurrentData();
					b_done = TRUE;
					break;
				}
			}
		}

		while (!b_done)
		{
			eventdata = mEventData.getNextEvent();
			if (eventdata)
			{
				event = eventdata->mType;

				// make sure that we can actually handle this one
				if (LSCRIPTStateBitField[event] & event_register)
				{
					//			push a zero to be popped
					lscript_push(mBuffer, 0);
		//			push sp as current bp
					S32 sp = get_register(mBuffer, LREG_SP);
					lscript_push(mBuffer, sp);

					set_event_register(mBuffer, LREG_IE, LSCRIPTStateBitField[event], major_version);
					current_events &= ~LSCRIPTStateBitField[event];
					set_event_register(mBuffer, LREG_CE, current_events, major_version);

	//				push any arguments that need to be pushed onto the stack
					// last piece of data will be type LST_NULL
					LLScriptLibData	*data = eventdata->mData;
					while (data->mType)
					{
						size += lscript_push_variable(data, mBuffer);
						data++;
					}
					b_done = TRUE;
		//			now, push any additional stack space
					S32 additional_size = get_event_stack_size(mBuffer, current_state, event) - size;
					lscript_pusharge(mBuffer, additional_size);

		//			now set the bp correctly
					sp = get_register(mBuffer, LREG_SP);
					sp += additional_size + size;
					set_bp(mBuffer, sp);
		//			set IP to the function
					S32			opcode_start = get_state_event_opcoode_start(mBuffer, current_state, event);
					set_ip(mBuffer, opcode_start);
				}
				else
				{
					llwarns << "Shit, somehow got an event that we're not registered for!" << llendl;
				}
				delete eventdata;
			}
			else
			{
//				if no data waiting, do it the old way:
				U64 handled_current = current_events & event_register;
				if (handled_current)
				{
					//			push a zero to be popped
					lscript_push(mBuffer, 0);
		//			push sp as current bp
					S32 sp = get_register(mBuffer, LREG_SP);
					lscript_push(mBuffer, sp);

					event = return_first_event((S32)handled_current);
					set_event_register(mBuffer, LREG_IE, LSCRIPTStateBitField[event], major_version);
					current_events &= ~LSCRIPTStateBitField[event];
					set_event_register(mBuffer, LREG_CE, current_events, major_version);
		//			now, push any additional stack space
					S32 additional_size = get_event_stack_size(mBuffer, current_state, event) - size;
					lscript_pusharge(mBuffer, additional_size);

		//			now set the bp correctly
					sp = get_register(mBuffer, LREG_SP);
					sp += additional_size + size;
					set_bp(mBuffer, sp);
		//			set IP to the function
					S32			opcode_start = get_state_event_opcoode_start(mBuffer, current_state, event);
					set_ip(mBuffer, opcode_start);
				}
				b_done = TRUE;
			}
		}

		return NO_DELETE_FLAG;
	}
}

BOOL run_noop(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tNOOP\n", offset);
	offset++;
	return FALSE;
}

BOOL run_pop(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOP\n", offset);
	offset++;
	lscript_poparg(buffer, LSCRIPTDataSize[LST_INTEGER]);
	return FALSE;
}

BOOL run_pops(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPS\n", offset);
	offset++;
	S32 address = lscript_pop_int(buffer);
	if (address)
		lsa_decrease_ref_count(buffer, address);
	return FALSE;
}

BOOL run_popl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPL\n", offset);
	offset++;
	S32 address = lscript_pop_int(buffer);
	if (address)
		lsa_decrease_ref_count(buffer, address);
	return FALSE;
}

BOOL run_popv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPV\n", offset);
	offset++;
	lscript_poparg(buffer, LSCRIPTDataSize[LST_VECTOR]);
	return FALSE;
}

BOOL run_popq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPQ\n", offset);
	offset++;
	lscript_poparg(buffer, LSCRIPTDataSize[LST_QUATERNION]);
	return FALSE;
}

BOOL run_poparg(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPARG ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", arg);
	lscript_poparg(buffer, arg);
	return FALSE;
}

BOOL run_popip(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPIP\n", offset);
	offset++;
	offset = lscript_pop_int(buffer);
	return FALSE;
}

BOOL run_popbp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPBP\n", offset);
	offset++;
	S32 bp = lscript_pop_int(buffer);
	set_bp(buffer, bp);
	return FALSE;
}

BOOL run_popsp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPSP\n", offset);
	offset++;
	S32 sp = lscript_pop_int(buffer);
	set_sp(buffer, sp);
	return FALSE;
}

BOOL run_popslr(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPOPSLR\n", offset);
	offset++;
	S32 slr = lscript_pop_int(buffer);
	set_register(buffer, LREG_SLR, slr);
	return FALSE;
}

BOOL run_dup(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tDUP\n", offset);
	offset++;
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_dups(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tDUPS\n", offset);
	offset++;
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);
	lscript_push(buffer, value);
	lsa_increase_ref_count(buffer, value);
	return FALSE;
}

BOOL run_dupl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tDUPL\n", offset);
	offset++;
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);
	lscript_push(buffer, value);
	lsa_increase_ref_count(buffer, value);
	return FALSE;
}

BOOL run_dupv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tDUPV\n", offset);
	offset++;
	S32 sp = get_register(buffer, LREG_SP);
	LLVector3 value;
	bytestream2vector(value, buffer, sp);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_dupq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tDUPV\n", offset);
	offset++;
	S32 sp = get_register(buffer, LREG_SP);
	LLQuaternion value;
	bytestream2quaternion(value, buffer, sp);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_store(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTORE ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);
	lscript_local_store(buffer, arg, value);
	return FALSE;
}

BOOL run_stores(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTORES ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);

	S32 address = lscript_local_get(buffer, arg);

	lscript_local_store(buffer, arg, value);
	lsa_increase_ref_count(buffer, value);
	if (address)
		lsa_decrease_ref_count(buffer, address);
	return FALSE;
}

BOOL run_storel(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREL ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);

	S32 address = lscript_local_get(buffer, arg);

	lscript_local_store(buffer, arg, value);
	lsa_increase_ref_count(buffer, value);
	if (address)
		lsa_decrease_ref_count(buffer, address);
	return FALSE;
}

BOOL run_storev(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREV ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLVector3 value;
	S32 sp = get_register(buffer, LREG_SP);
	bytestream2vector(value, buffer, sp);
	lscript_local_store(buffer, arg, value);
	return FALSE;
}

BOOL run_storeq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREQ ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLQuaternion value;
	S32 sp = get_register(buffer, LREG_SP);
	bytestream2quaternion(value, buffer, sp);
	lscript_local_store(buffer, arg, value);
	return FALSE;
}

BOOL run_storeg(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREG ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);
	lscript_global_store(buffer, arg, value);
	return FALSE;
}

BOOL run_storegs(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGS ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);

	S32 address = lscript_global_get(buffer, arg);

	lscript_global_store(buffer, arg, value);

	lsa_increase_ref_count(buffer, value);
	if (address)
		lsa_decrease_ref_count(buffer, address);
	return FALSE;
}

BOOL run_storegl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGL ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 sp = get_register(buffer, LREG_SP);
	S32 value = bytestream2integer(buffer, sp);

	S32 address = lscript_global_get(buffer, arg);

	lscript_global_store(buffer, arg, value);

	lsa_increase_ref_count(buffer, value);
	if (address)
		lsa_decrease_ref_count(buffer, address);
	return FALSE;
}

BOOL run_storegv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGV ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLVector3 value;
	S32 sp = get_register(buffer, LREG_SP);
	bytestream2vector(value, buffer, sp);
	lscript_global_store(buffer, arg, value);
	return FALSE;
}

BOOL run_storegq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGQ ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLQuaternion value;
	S32 sp = get_register(buffer, LREG_SP);
	bytestream2quaternion(value, buffer, sp);
	lscript_global_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_pop_int(buffer);
	lscript_local_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadsp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTORESP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_pop_int(buffer);

	S32 address = lscript_local_get(buffer, arg);
	if (address)
		lsa_decrease_ref_count(buffer, address);

	lscript_local_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadlp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTORELP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_pop_int(buffer);

	S32 address = lscript_local_get(buffer, arg);
	if (address)
		lsa_decrease_ref_count(buffer, address);

	lscript_local_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadvp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREVP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLVector3 value;
	lscript_pop_vector(buffer, value);
	lscript_local_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadqp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREQP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLQuaternion value;
	lscript_pop_quaternion(buffer, value);
	lscript_local_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadgp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_pop_int(buffer);
	lscript_global_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadgsp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGSP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", arg);
	S32 value = lscript_pop_int(buffer);

	S32 address = lscript_global_get(buffer, arg);
	if (address)
		lsa_decrease_ref_count(buffer, address);
	
	lscript_global_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadglp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGLP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_pop_int(buffer);

	S32 address = lscript_global_get(buffer, arg);
	if (address)
		lsa_decrease_ref_count(buffer, address);
	
	lscript_global_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadgvp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGVP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLVector3 value;
	lscript_pop_vector(buffer, value);
	lscript_global_store(buffer, arg, value);
	return FALSE;
}

BOOL run_loadgqp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTOREGQP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLQuaternion value;
	lscript_pop_quaternion(buffer, value);
	lscript_global_store(buffer, arg, value);
	return FALSE;
}

BOOL run_push(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSH ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_local_get(buffer, arg);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_pushs(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHS ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_local_get(buffer, arg);
	lscript_push(buffer, value);
	lsa_increase_ref_count(buffer, value);
	return FALSE;
}

BOOL run_pushl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHL ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_local_get(buffer, arg);
	lscript_push(buffer, value);
	lsa_increase_ref_count(buffer, value);
	return FALSE;
}

BOOL run_pushv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHV ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLVector3 value;
	lscript_local_get(buffer, arg, value);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_pushq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHQ ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLQuaternion value;
	lscript_local_get(buffer, arg, value);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_pushg(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHG ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_global_get(buffer, arg);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_pushgs(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHGS ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_global_get(buffer, arg);
	lscript_push(buffer, value);
	lsa_increase_ref_count(buffer, value);
	return FALSE;
}

BOOL run_pushgl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHGL ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	S32 value = lscript_global_get(buffer, arg);
	lscript_push(buffer, value);
	lsa_increase_ref_count(buffer, value);
	return FALSE;
}

BOOL run_pushgv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHGV ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLVector3 value;
	lscript_global_get(buffer, arg, value);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_pushgq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHGQ ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("0x%X\n", arg);
	LLQuaternion value;
	lscript_global_get(buffer, arg, value);
	lscript_push(buffer, value);
	return FALSE;
}

BOOL run_puship(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHIP\n", offset);
	offset++;
	lscript_push(buffer, offset);
	return FALSE;
}

BOOL run_pushbp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHBP\n", offset);
	offset++;
	lscript_push(buffer, get_register(buffer, LREG_BP));
	return FALSE;
}

BOOL run_pushsp(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHSP\n", offset);
	offset++;
	lscript_push(buffer, get_register(buffer, LREG_SP));
	return FALSE;
}

BOOL run_pushargb(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHGARGB ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	if (b_print)
		printf("%d\n", (U32)arg);
	lscript_push(buffer, arg);
	return FALSE;
}

BOOL run_pushargi(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHARGI ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", arg);
	lscript_push(buffer, arg);
	return FALSE;
}

BOOL run_pushargf(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHARGF ", offset);
	offset++;
	F32 arg = safe_instruction_bytestream2float(buffer, offset);
	if (b_print)
		printf("%f\n", arg);
	lscript_push(buffer, arg);
	return FALSE;
}

BOOL run_pushargs(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHARGS ", offset);
	S32 toffset = offset;
	safe_instruction_bytestream_count_char(buffer, toffset);
	S32 size = toffset - offset;
	char *arg = new char[size];
	offset++;
	safe_instruction_bytestream2char(arg, buffer, offset);
	if (b_print)
		printf("%s\n", arg);
	S32 address = lsa_heap_add_data(buffer, new LLScriptLibData(arg), get_max_heap_size(buffer), TRUE);
	lscript_push(buffer, address);
	delete [] arg;
	return FALSE;
}

BOOL run_pushargv(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHARGV ", offset);
	offset++;
	LLVector3 arg;
	safe_instruction_bytestream2vector(arg, buffer, offset);
	if (b_print)
		printf("< %f, %f, %f >\n", arg.mV[VX], arg.mV[VY], arg.mV[VZ]);
	lscript_push(buffer, arg);
	return FALSE;
}
BOOL run_pushargq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHARGQ ", offset);
	offset++;
	LLQuaternion arg;
	safe_instruction_bytestream2quaternion(arg, buffer, offset);
	if (b_print)
		printf("< %f, %f, %f, %f >\n", arg.mQ[VX], arg.mQ[VY], arg.mQ[VZ], arg.mQ[VS]);
	lscript_push(buffer, arg);
	return FALSE;
}
BOOL run_pushe(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHE\n", offset);
	offset++;
	lscript_pusharge(buffer, LSCRIPTDataSize[LST_INTEGER]);
	return FALSE;
}
BOOL run_pushev(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHEV\n", offset);
	offset++;
	lscript_pusharge(buffer, LSCRIPTDataSize[LST_VECTOR]);
	return FALSE;
}
BOOL run_pusheq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHEQ\n", offset);
	offset++;
	lscript_pusharge(buffer, LSCRIPTDataSize[LST_QUATERNION]);
	return FALSE;
}
BOOL run_pusharge(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPUSHARGE ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", arg);
	lscript_pusharge(buffer, arg);
	return FALSE;
}

void print_type(U8 type)
{
	if (type == LSCRIPTTypeByte[LST_INTEGER])
	{
		printf("integer");
	}
	else if (type == LSCRIPTTypeByte[LST_FLOATINGPOINT])
	{
		printf("float");
	}
	else if (type == LSCRIPTTypeByte[LST_STRING])
	{
		printf("string");
	}
	else if (type == LSCRIPTTypeByte[LST_KEY])
	{
		printf("key");
	}
	else if (type == LSCRIPTTypeByte[LST_VECTOR])
	{
		printf("vector");
	}
	else if (type == LSCRIPTTypeByte[LST_QUATERNION])
	{
		printf("quaternion");
	}
	else if (type == LSCRIPTTypeByte[LST_LIST])
	{
		printf("list");
	}
}

void unknown_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	printf("Unknown arithmetic operation!\n");
}

void integer_integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 result = 0;

	switch(opcode)
	{
	case LOPC_ADD:
		result = lside + rside;
		break;
	case LOPC_SUB:
		result = lside - rside;
		break;
	case LOPC_MUL:
		result = lside * rside;
		break;
	case LOPC_DIV:
		if (rside)
			result = lside / rside;
		else
			set_fault(buffer, LSRF_MATH);
		break;
	case LOPC_MOD:
		if (rside)
			result = lside % rside;
		else
			set_fault(buffer, LSRF_MATH);
		break;
	case LOPC_EQ:
		result = (lside == rside);
		break;
	case LOPC_NEQ:
		result = (lside != rside);
		break;
	case LOPC_LEQ:
		result = (lside <= rside);
		break;
	case LOPC_GEQ:
		result = (lside >= rside);
		break;
	case LOPC_LESS:
		result = (lside < rside);
		break;
	case LOPC_GREATER:
		result = (lside > rside);
		break;
	case LOPC_BITAND:
		result = (lside & rside);
		break;
	case LOPC_BITOR:
		result = (lside | rside);
		break;
	case LOPC_BITXOR:
		result = (lside ^ rside);
		break;
	case LOPC_BOOLAND:
		result = (lside && rside);
		break;
	case LOPC_BOOLOR:
		result = (lside || rside);
		break;
	case LOPC_SHL:
		result = (lside << rside);
		break;
	case LOPC_SHR:
		result = (lside >> rside);
		break;
	default:
		break;
	}
	lscript_push(buffer, result);
}

void integer_float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	F32 rside = lscript_pop_float(buffer);
	S32 resulti = 0;
	F32 resultf = 0;

	switch(opcode)
	{
	case LOPC_ADD:
		resultf = lside + rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_SUB:
		resultf = lside - rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_MUL:
		resultf = lside * rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_DIV:
		if (rside)
			resultf = lside / rside;
		else
			set_fault(buffer, LSRF_MATH);
		lscript_push(buffer, resultf);
		break;
	case LOPC_EQ:
		resulti = (lside == rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_NEQ:
		resulti = (lside != rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_LEQ:
		resulti = (lside <= rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_GEQ:
		resulti = (lside >= rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_LESS:
		resulti = (lside < rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_GREATER:
		resulti = (lside > rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void integer_vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	LLVector3 rside;
	lscript_pop_vector(buffer, rside);

	switch(opcode)
	{
	case LOPC_MUL:
		rside *= (F32)lside;
		lscript_push(buffer, rside);
		break;
	default:
		break;
	}
}

void float_integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	F32 lside = lscript_pop_float(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 resulti = 0;
	F32 resultf = 0;

	switch(opcode)
	{
	case LOPC_ADD:
		resultf = lside + rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_SUB:
		resultf = lside - rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_MUL:
		resultf = lside * rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_DIV:
		if (rside)
			resultf = lside / rside;
		else
			set_fault(buffer, LSRF_MATH);
		lscript_push(buffer, resultf);
		break;
	case LOPC_EQ:
		resulti = (lside == rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_NEQ:
		resulti = (lside != rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_LEQ:
		resulti = (lside <= rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_GEQ:
		resulti = (lside >= rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_LESS:
		resulti = (lside < rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_GREATER:
		resulti = (lside > rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void float_float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	F32 lside = lscript_pop_float(buffer);
	F32 rside = lscript_pop_float(buffer);
	F32 resultf = 0;
	S32 resulti = 0;

	switch(opcode)
	{
	case LOPC_ADD:
		resultf = lside + rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_SUB:
		resultf = lside - rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_MUL:
		resultf = lside * rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_DIV:
		if (rside)
			resultf = lside / rside;
		else
			set_fault(buffer, LSRF_MATH);
		lscript_push(buffer, resultf);
		break;
	case LOPC_EQ:
		resulti = (lside == rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_NEQ:
		resulti = (lside != rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_LEQ:
		resulti = (lside <= rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_GEQ:
		resulti = (lside >= rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_LESS:
		resulti = (lside < rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_GREATER:
		resulti = (lside > rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void float_vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	F32 lside = lscript_pop_float(buffer);
	LLVector3 rside;
	lscript_pop_vector(buffer, rside);

	switch(opcode)
	{
	case LOPC_MUL:
		rside *= lside;
		lscript_push(buffer, rside);
		break;
	default:
		break;
	}
}

void string_string_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 resulti;
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		address = lsa_cat_strings(buffer, lside, rside, get_max_heap_size(buffer));
		lscript_push(buffer, address);
		break;
	case LOPC_EQ:
		resulti = !lsa_cmp_strings(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_NEQ:
		resulti = lsa_cmp_strings(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void string_key_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 resulti;

	switch(opcode)
	{
	case LOPC_NEQ:
		resulti = lsa_cmp_strings(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_EQ:
		resulti = !lsa_cmp_strings(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void key_string_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 resulti;

	switch(opcode)
	{
	case LOPC_NEQ:
		resulti = lsa_cmp_strings(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_EQ:
		resulti = !lsa_cmp_strings(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void key_key_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 resulti;

	switch(opcode)
	{
	case LOPC_EQ:
		resulti = !lsa_cmp_strings(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_NEQ:
		resulti = lsa_cmp_strings(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void vector_integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLVector3 lside;
	lscript_pop_vector(buffer, lside);
	S32 rside = lscript_pop_int(buffer);

	switch(opcode)
	{
	case LOPC_MUL:
		lside *= (F32)rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_DIV:
		if (rside)
			lside *= (1.f/rside);
		else
			set_fault(buffer, LSRF_MATH);
		lscript_push(buffer, lside);
		break;
	default:
		break;
	}
}

void vector_float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLVector3 lside;
	lscript_pop_vector(buffer, lside);
	F32 rside = lscript_pop_float(buffer);

	switch(opcode)
	{
	case LOPC_MUL:
		lside *= rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_DIV:
		if (rside)
			lside *= (1.f/rside);
		else
			set_fault(buffer, LSRF_MATH);
		lscript_push(buffer, lside);
		break;
	default:
		break;
	}
}

void vector_vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLVector3 lside;
	lscript_pop_vector(buffer, lside);
	LLVector3 rside;
	lscript_pop_vector(buffer, rside);
	S32 resulti = 0;
	F32 resultf = 0.f;

	switch(opcode)
	{
	case LOPC_ADD:
		lside += rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_SUB:
		lside -= rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_MUL:
		resultf = lside * rside;
		lscript_push(buffer, resultf);
		break;
	case LOPC_MOD:
		lside = lside % rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_EQ:
		resulti = (lside == rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_NEQ:
		resulti = (lside != rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void vector_quaternion_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLVector3 lside;
	lscript_pop_vector(buffer, lside);
	LLQuaternion rside;
	lscript_pop_quaternion(buffer, rside);

	switch(opcode)
	{
	case LOPC_MUL:
		lside = lside * rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_DIV:
		lside = lside * rside.conjQuat();
		lscript_push(buffer, lside);
		break;
	default:
		break;
	}
}

void quaternion_quaternion_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLQuaternion lside;
	lscript_pop_quaternion(buffer, lside);
	LLQuaternion rside;
	lscript_pop_quaternion(buffer, rside);
	S32 resulti = 0;

	switch(opcode)
	{
	case LOPC_ADD:
		lside = lside + rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_SUB:
		lside = lside - rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_MUL:
		lside *= rside;
		lscript_push(buffer, lside);
		break;
	case LOPC_DIV:
		lside = lside * rside.conjQuat();
		lscript_push(buffer, lside);
		break;
	case LOPC_EQ:
		resulti = (lside == rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_NEQ:
		resulti = (lside != rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

void integer_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = new LLScriptLibData(lside);
			address = lsa_preadd_lists(buffer, list, rside, get_max_heap_size(buffer));
			lscript_push(buffer, address);
			list->mListp = NULL;
			delete list;
		}
		break;
	default:
		break;
	}
}

void float_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	F32 lside = lscript_pop_float(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = new LLScriptLibData(lside);
			address = lsa_preadd_lists(buffer, list, rside, get_max_heap_size(buffer));
			lscript_push(buffer, address);
			list->mListp = NULL;
			delete list;
		}
		break;
	default:
		break;
	}
}

void string_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *string = lsa_get_data(buffer, lside, TRUE);
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = string;
			address = lsa_preadd_lists(buffer, list, rside, get_max_heap_size(buffer));
			lscript_push(buffer, address);
			list->mListp = NULL;
			delete list;
		}
		break;
	default:
		break;
	}
}

void key_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *key = lsa_get_data(buffer, lside, TRUE);
			// need to convert key to key, since it comes out like a string
			if (key->mType == LST_STRING)
			{
				key->mKey = key->mString;
				key->mString = NULL;
				key->mType = LST_KEY;
			}
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = key;
			address = lsa_preadd_lists(buffer, list, rside, get_max_heap_size(buffer));
			lscript_push(buffer, address);
			list->mListp = NULL;
			delete list;
		}
		break;
	default:
		break;
	}
}

void vector_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLVector3 lside;
	lscript_pop_vector(buffer, lside);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = new LLScriptLibData(lside);
			address = lsa_preadd_lists(buffer, list, rside, get_max_heap_size(buffer));
			lscript_push(buffer, address);
			list->mListp = NULL;
			delete list;
		}
		break;
	default:
		break;
	}
}

void quaternion_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLQuaternion lside;
	lscript_pop_quaternion(buffer, lside);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = new LLScriptLibData(lside);
			address = lsa_preadd_lists(buffer, list, rside, get_max_heap_size(buffer));
			lscript_push(buffer, address);
			list->mListp = NULL;
			delete list;
		}
		break;
	default:
		break;
	}
}

void list_integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = new LLScriptLibData(rside);
			address = lsa_postadd_lists(buffer, lside, list, get_max_heap_size(buffer));
			list->mListp = NULL;
			delete list;
			lscript_push(buffer, address);
		}
		break;
	default:
		break;
	}
}

void list_float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	F32 rside = lscript_pop_float(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = new LLScriptLibData(rside);
			address = lsa_postadd_lists(buffer, lside, list, get_max_heap_size(buffer));
			list->mListp = NULL;
			delete list;
			lscript_push(buffer, address);
		}
		break;
	default:
		break;
	}
}

void list_string_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *string = lsa_get_data(buffer, rside, TRUE);
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = string;
			address = lsa_postadd_lists(buffer, lside, list, get_max_heap_size(buffer));
			list->mListp = NULL;
			delete list;
			lscript_push(buffer, address);
		}
		break;
	default:
		break;
	}
}

void list_key_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *key = lsa_get_data(buffer, rside, TRUE);
			// need to convert key to key, since it comes out like a string
			if (key->mType == LST_STRING)
			{
				key->mKey = key->mString;
				key->mString = NULL;
				key->mType = LST_KEY;
			}
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = key;
			address = lsa_postadd_lists(buffer, lside, list, get_max_heap_size(buffer));
			list->mListp = NULL;
			delete list;
			lscript_push(buffer, address);
		}
		break;
	default:
		break;
	}
}

void list_vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	LLVector3 rside;
	lscript_pop_vector(buffer, rside);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = new LLScriptLibData(rside);
			address = lsa_postadd_lists(buffer, lside, list, get_max_heap_size(buffer));
			list->mListp = NULL;
			delete list;
			lscript_push(buffer, address);
		}
		break;
	default:
		break;
	}
}

void list_quaternion_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	LLQuaternion rside;
	lscript_pop_quaternion(buffer, rside);
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		{
			LLScriptLibData *list = new LLScriptLibData;
			list->mType = LST_LIST;
			list->mListp = new LLScriptLibData(rside);
			address = lsa_postadd_lists(buffer, lside, list, get_max_heap_size(buffer));
			list->mListp = NULL;
			delete list;
			lscript_push(buffer, address);
		}
		break;
	default:
		break;
	}
}

void list_list_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 rside = lscript_pop_int(buffer);
	S32 resulti;
	S32 address;

	switch(opcode)
	{
	case LOPC_ADD:
		address = lsa_cat_lists(buffer, lside, rside, get_max_heap_size(buffer));
		lscript_push(buffer, address);
		break;
	case LOPC_EQ:
		resulti = !lsa_cmp_lists(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	case LOPC_NEQ:
		resulti = lsa_cmp_lists(buffer, lside, rside);
		lscript_push(buffer, resulti);
		break;
	default:
		break;
	}
}

BOOL run_add(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tADD ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_ADD);
	return FALSE;
}

BOOL run_sub(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSUB ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_SUB);
	return FALSE;
}
BOOL run_mul(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tMUL ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_MUL);
	return FALSE;
}
BOOL run_div(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tDIV ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_DIV);
	return FALSE;
}
BOOL run_mod(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tMOD ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_MOD);
	return FALSE;
}

BOOL run_eq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tEQ ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_EQ);
	return FALSE;
}
BOOL run_neq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tNEQ ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_NEQ);
	return FALSE;
}
BOOL run_leq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tLEQ ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_LEQ);
	return FALSE;
}
BOOL run_geq(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tGEQ ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_GEQ);
	return FALSE;
}
BOOL run_less(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tLESS ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_LESS);
	return FALSE;
}
BOOL run_greater(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tGREATER ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 arg1 = arg >> 4;
	U8 arg2 = arg & 0xf;
	if (b_print)
	{
		print_type(arg1);
		printf(", ");
		print_type(arg2);
		printf("\n");
	}
	binary_operations[arg1][arg2](buffer, LOPC_GREATER);
	return FALSE;
}

BOOL run_bitand(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tBITAND\n", offset);
	offset++;
	binary_operations[LST_INTEGER][LST_INTEGER](buffer, LOPC_BITAND);
	return FALSE;
}
BOOL run_bitor(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tBITOR\n", offset);
	offset++;
	binary_operations[LST_INTEGER][LST_INTEGER](buffer, LOPC_BITOR);
	return FALSE;
}
BOOL run_bitxor(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tBITXOR\n", offset);
	offset++;
	binary_operations[LST_INTEGER][LST_INTEGER](buffer, LOPC_BITXOR);
	return FALSE;
}
BOOL run_booland(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tBOOLAND\n", offset);
	offset++;
	binary_operations[LST_INTEGER][LST_INTEGER](buffer, LOPC_BOOLAND);
	return FALSE;
}
BOOL run_boolor(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tBOOLOR\n", offset);
	offset++;
	binary_operations[LST_INTEGER][LST_INTEGER](buffer, LOPC_BOOLOR);
	return FALSE;
}

BOOL run_shl(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSHL\n", offset);
	offset++;
	binary_operations[LST_INTEGER][LST_INTEGER](buffer, LOPC_SHL);
	return FALSE;
}
BOOL run_shr(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSHR\n", offset);
	offset++;
	binary_operations[LST_INTEGER][LST_INTEGER](buffer, LOPC_SHR);
	return FALSE;
}

void integer_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	S32 lside = lscript_pop_int(buffer);
	S32 result = 0;

	switch(opcode)
	{
	case LOPC_NEG:
		result = -lside;
		break;
	case LOPC_BITNOT:
		result = ~lside;
		break;
	case LOPC_BOOLNOT:
		result = !lside;
		break;
	default:
		break;
	}
	lscript_push(buffer, result);
}

void float_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	F32 lside = lscript_pop_float(buffer);
	F32 result = 0;

	switch(opcode)
	{
	case LOPC_NEG:
		result = -lside;
		lscript_push(buffer, result);
		break;
	default:
		break;
	}
}

void vector_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLVector3 lside;
	lscript_pop_vector(buffer, lside);
	LLVector3 result;

	switch(opcode)
	{
	case LOPC_NEG:
		result = -lside;
		lscript_push(buffer, result);
		break;
	default:
		break;
	}
}

void quaternion_operation(U8 *buffer, LSCRIPTOpCodesEnum opcode)
{
	LLQuaternion lside;
	lscript_pop_quaternion(buffer, lside);
	LLQuaternion result;

	switch(opcode)
	{
	case LOPC_NEG:
		result = -lside;
		lscript_push(buffer, result);
		break;
	default:
		break;
	}
}


BOOL run_neg(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tNEG ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	if (b_print)
	{
		print_type(arg);
		printf("\n");
	}
	unary_operations[arg](buffer, LOPC_NEG);
	return FALSE;
}

BOOL run_bitnot(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tBITNOT\n", offset);
	offset++;
	unary_operations[LST_INTEGER](buffer, LOPC_BITNOT);
	return FALSE;
}

BOOL run_boolnot(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tBOOLNOT\n", offset);
	offset++;
	unary_operations[LST_INTEGER](buffer, LOPC_BOOLNOT);
	return FALSE;
}

BOOL run_jump(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tJUMP ", offset);
	offset++;
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", arg);
	offset += arg;
	return FALSE;
}
BOOL run_jumpif(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tJUMPIF ", offset);
	offset++;
	U8 type = safe_instruction_bytestream2byte(buffer, offset);
	if (b_print)
	{
		print_type(type);
		printf(", ");
	}
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", arg);

	if (type == LST_INTEGER)
	{
		S32 test = lscript_pop_int(buffer);
		if (test)
		{
			offset += arg;
		}
	}
	else if (type == LST_FLOATINGPOINT)
	{
		F32 test = lscript_pop_float(buffer);
		if (test)
		{
			offset += arg;
		}
	}
	else if (type == LST_VECTOR)
	{
		LLVector3 test;
		lscript_pop_vector(buffer, test);
		if (!test.isExactlyZero())
		{
			offset += arg;
		}
	}
	else if (type == LST_QUATERNION)
	{
		LLQuaternion test;
		lscript_pop_quaternion(buffer, test);
		if (!test.isIdentity())
		{
			offset += arg;
		}
	}
	else if (type == LST_STRING)
	{
		S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
		S32 address = base_address + get_register(buffer, LREG_HR) - 1;
		if (address)
		{
			S32 string = address;
			string += SIZEOF_SCRIPT_ALLOC_ENTRY;
			if (safe_heap_check_address(buffer, string, 1))
			{
				S32 toffset = string;
				safe_heap_bytestream_count_char(buffer, toffset);
				S32 size = toffset - string;
				char *sdata = new char[size];
				bytestream2char(sdata, buffer, string);
				if (strlen(sdata))
				{
					offset += arg;
				}
				delete [] sdata;
			}
			lsa_decrease_ref_count(buffer, base_address);
		}
	}
	else if (type == LST_KEY)
	{
		S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
		S32 address = base_address + get_register(buffer, LREG_HR) - 1;
		if (address)
		{
			S32 string = address;
			string += SIZEOF_SCRIPT_ALLOC_ENTRY;
			if (safe_heap_check_address(buffer, string, 1))
			{
				S32 toffset = string;
				safe_heap_bytestream_count_char(buffer, toffset);
				S32 size = toffset - string;
				char *sdata = new char[size];
				bytestream2char(sdata, buffer, string);
				if (strlen(sdata))
				{
					LLUUID id;
					id.set(sdata);
					if (id != LLUUID::null)
						offset += arg;
				}
				delete [] sdata;
			}
			lsa_decrease_ref_count(buffer, base_address);
		}
		else if (type == LST_LIST)
		{
			S32 address = lscript_pop_int(buffer);
			LLScriptLibData *list = lsa_get_data(buffer, address, TRUE);
			if (list->getListLength())
			{
				offset += arg;
			}
		}
	}
	return FALSE;
}
BOOL run_jumpnif(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tJUMPNIF ", offset);
	offset++;
	U8 type = safe_instruction_bytestream2byte(buffer, offset);
	if (b_print)
	{
		print_type(type);
		printf(", ");
	}
	S32 arg = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", arg);

	if (type == LST_INTEGER)
	{
		S32 test = lscript_pop_int(buffer);
		if (!test)
		{
			offset += arg;
		}
	}
	else if (type == LST_FLOATINGPOINT)
	{
		F32 test = lscript_pop_float(buffer);
		if (!test)
		{
			offset += arg;
		}
	}
	else if (type == LST_VECTOR)
	{
		LLVector3 test;
		lscript_pop_vector(buffer, test);
		if (test.isExactlyZero())
		{
			offset += arg;
		}
	}
	else if (type == LST_QUATERNION)
	{
		LLQuaternion test;
		lscript_pop_quaternion(buffer, test);
		if (test.isIdentity())
		{
			offset += arg;
		}
	}
	else if (type == LST_STRING)
	{
		S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
		S32 address = base_address + get_register(buffer, LREG_HR) - 1;
		if (address)
		{
			S32 string = address;
			string += SIZEOF_SCRIPT_ALLOC_ENTRY;
			if (safe_heap_check_address(buffer, string, 1))
			{
				S32 toffset = string;
				safe_heap_bytestream_count_char(buffer, toffset);
				S32 size = toffset - string;
				char *sdata = new char[size];
				bytestream2char(sdata, buffer, string);
				if (!strlen(sdata))
				{
					offset += arg;
				}
				delete [] sdata;
			}
			lsa_decrease_ref_count(buffer, base_address);
		}
	}
	else if (type == LST_KEY)
	{
		S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
		S32 address = base_address + get_register(buffer, LREG_HR) - 1;
		if (address)
		{
			S32 string = address;
			string += SIZEOF_SCRIPT_ALLOC_ENTRY;
			if (safe_heap_check_address(buffer, string, 1))
			{
				S32 toffset = string;
				safe_heap_bytestream_count_char(buffer, toffset);
				S32 size = toffset - string;
				char *sdata = new char[size];
				bytestream2char(sdata, buffer, string);
				if (strlen(sdata))
				{
					LLUUID id;
					id.set(sdata);
					if (id == LLUUID::null)
						offset += arg;
				}
				else
				{
					offset += arg;
				}
				delete [] sdata;
			}
			lsa_decrease_ref_count(buffer, base_address);
		}
		else if (type == LST_LIST)
		{
			S32 address = lscript_pop_int(buffer);
			LLScriptLibData *list = lsa_get_data(buffer, address, TRUE);
			if (!list->getListLength())
			{
				offset += arg;
			}
		}
	}
	return FALSE;
}

BOOL run_state(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tSTATE ", offset);
	offset++;
	S32 state = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", state);

	S32 bp = lscript_pop_int(buffer);
	set_bp(buffer, bp);

	offset = lscript_pop_int(buffer);

	S32 major_version = 0;
	S32 value = get_register(buffer, LREG_VN);
	if (value == LSL2_VERSION1_END_NUMBER)
	{
		major_version = 1;
	}
	else if (value == LSL2_VERSION_NUMBER)
	{
		major_version = 2;
	}

	S32 current_state = get_register(buffer, LREG_CS);
	if (state != current_state)
	{
		U64 ce = get_event_register(buffer, LREG_CE, major_version);
		ce |= LSCRIPTStateBitField[LSTT_STATE_EXIT];
		set_event_register(buffer, LREG_CE, ce, major_version);
	}
	set_register(buffer, LREG_NS, state);
	return FALSE;
}

BOOL run_call(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tCALL ", offset);
	offset++;
	S32 func = safe_instruction_bytestream2integer(buffer, offset);
	if (b_print)
		printf("%d\n", func);

	lscript_local_store(buffer, -8, offset);

	S32 minimum = get_register(buffer, LREG_GFR);
	S32 maximum = get_register(buffer, LREG_SR);
	S32 lookup = minimum + func*4 + 4;
	S32 function;

	if (  (lookup >= minimum)
		&&(lookup < maximum))
	{
		function = bytestream2integer(buffer, lookup) + minimum;
		if (  (lookup >= minimum)
			&&(lookup < maximum))
		{
			offset = function;
			offset += bytestream2integer(buffer, function);
		}
		else
		{
			set_fault(buffer, LSRF_BOUND_CHECK_ERROR);
		}
	}
	else
	{
		set_fault(buffer, LSRF_BOUND_CHECK_ERROR);
	}
	return FALSE;
}

BOOL run_return(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tRETURN\n", offset);
	offset++;
	S32 bp = lscript_pop_int(buffer);
	set_bp(buffer, bp);
	offset = lscript_pop_int(buffer);
	return FALSE;
}

S32 axtoi(char *hexStg) 
{
  S32 n = 0;         // position in string
  S32 m = 0;         // position in digit[] to shift
  S32 count;         // loop index
  S32 intValue = 0;  // integer value of hex string
  S32 digit[9];      // hold values to convert
  while (n < 8) 
  {
     if (hexStg[n]=='\0')
        break;
     if (hexStg[n] > 0x29 && hexStg[n] < 0x40 ) //if 0 to 9
        digit[n] = hexStg[n] & 0x0f;            //convert to int
     else if (hexStg[n] >='a' && hexStg[n] <= 'f') //if a to f
        digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
     else if (hexStg[n] >='A' && hexStg[n] <= 'F') //if A to F
        digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
     else break;
    n++;
  }
  count = n;
  m = n - 1;
  n = 0;
  while(n < count) 
  {
     // digit[n] is value of hex digit at position n
     // (m << 2) is the number of positions to shift
     // OR the bits into return value
     intValue = intValue | (digit[n] << (m << 2));
     m--;   // adjust the position to set
     n++;   // next digit to process
  }
  return (intValue);
}


BOOL run_cast(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	char caststr[1024];
	if (b_print)
		printf("[0x%X]\tCAST ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	U8 from = arg >> 4;
	U8 to = arg & 0xf;
	if (b_print)
	{
		print_type(from);
		printf(", ");
		print_type(to);
		printf("\n");
	}

	switch(from)
	{
	case LST_INTEGER:
		{
			switch(to)
			{
			case LST_INTEGER:
				break;
			case LST_FLOATINGPOINT:
				{
					S32 source = lscript_pop_int(buffer);
					F32 dest = (F32)source;
					lscript_push(buffer, dest);
				}
				break;
			case LST_STRING:
				{
					S32 address, source = lscript_pop_int(buffer);
					sprintf(caststr, "%d", source);
					address = lsa_heap_add_data(buffer, new LLScriptLibData(caststr), get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			case LST_LIST:
				{
					S32 address, source = lscript_pop_int(buffer);
					LLScriptLibData *list = new LLScriptLibData;
					list->mType = LST_LIST;
					list->mListp = new LLScriptLibData(source);
					address = lsa_heap_add_data(buffer, list, get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			default:
				break;
			}
		}
		break;
	case LST_FLOATINGPOINT:
		{
			switch(to)
			{
			case LST_INTEGER:
				{
					F32 source = lscript_pop_float(buffer);
					S32 dest = (S32)source;
					lscript_push(buffer, dest);
				}
				break;
			case LST_FLOATINGPOINT:
				break;
			case LST_STRING:
				{
					S32 address;
					F32 source = lscript_pop_float(buffer);
					sprintf(caststr, "%f", source);
					address = lsa_heap_add_data(buffer, new LLScriptLibData(caststr), get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			case LST_LIST:
				{
					S32 address;
					F32 source = lscript_pop_float(buffer);
					LLScriptLibData *list = new LLScriptLibData;
					list->mType = LST_LIST;
					list->mListp = new LLScriptLibData(source);
					address = lsa_heap_add_data(buffer, list, get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			default:
				break;
			}
		}
		break;
	case LST_STRING:
		{
			switch(to)
			{
			case LST_INTEGER:
				{
					S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
					S32 address = base_address + get_register(buffer, LREG_HR) - 1;
					if (address)
					{
						S32 string = address;
						string += SIZEOF_SCRIPT_ALLOC_ENTRY;
						if (safe_heap_check_address(buffer, string, 1))
						{
							S32 toffset = string;
							safe_heap_bytestream_count_char(buffer, toffset);
							S32 size = toffset - string;
							char *arg = new char[size];
							bytestream2char(arg, buffer, string);
							// S32 length = strlen(arg);
							S32 dest;
							S32 base;

							// Check to see if this is a hexidecimal number.
							if (  (arg[0] == '0') &&
								  (arg[1] == 'x' || arg[1] == 'X') )
							{
								// Let strtoul do a hex conversion.
								base = 16;
							}
							else
							{
								// Force base-10, so octal is never used.
								base = 10;
							}

							dest = strtoul(arg, NULL, base);

							lscript_push(buffer, dest);
							delete [] arg;
						}
						lsa_decrease_ref_count(buffer, base_address);
					}
				}
				break;
			case LST_FLOATINGPOINT:
				{
					S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
					S32 address = base_address + get_register(buffer, LREG_HR) - 1;
					if (address)
					{
						S32 string = address;
						string += SIZEOF_SCRIPT_ALLOC_ENTRY;
						if (safe_heap_check_address(buffer, string, 1))
						{
							S32 toffset = string;
							safe_heap_bytestream_count_char(buffer, toffset);
							S32 size = toffset - string;
							char *arg = new char[size];
							bytestream2char(arg, buffer, string);
							F32 dest = (F32)atof(arg);

							
							lscript_push(buffer, dest);
							delete [] arg;
						}
						lsa_decrease_ref_count(buffer, base_address);
					}
				}
				break;
			case LST_STRING:
				break;
			case LST_LIST:
				{
					S32 saddress = lscript_pop_int(buffer);
					LLScriptLibData *string = lsa_get_data(buffer, saddress, TRUE);
					LLScriptLibData *list = new LLScriptLibData;
					list->mType = LST_LIST;
					list->mListp = string;
					S32 address = lsa_heap_add_data(buffer, list, get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			case LST_VECTOR:
				{
					S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
					S32 address = base_address + get_register(buffer, LREG_HR) - 1;
					if (address)
					{
						S32 string = address;
						string += SIZEOF_SCRIPT_ALLOC_ENTRY;
						if (safe_heap_check_address(buffer, string, 1))
						{
							S32 toffset = string;
							safe_heap_bytestream_count_char(buffer, toffset);
							S32 size = toffset - string;
							char *arg = new char[size];
							bytestream2char(arg, buffer, string);
							LLVector3 vec;
							S32 num = sscanf(arg, "<%f, %f, %f>", &vec.mV[VX], &vec.mV[VY], &vec.mV[VZ]);
							if (num != 3)
							{
								vec = LLVector3::zero;
							}
							lscript_push(buffer, vec);
							delete [] arg;
						}
						lsa_decrease_ref_count(buffer, base_address);
					}
				}
				break;
			case LST_QUATERNION:
				{
					S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
					S32 address = base_address + get_register(buffer, LREG_HR) - 1;
					if (address)
					{
						S32 string = address;
						string += SIZEOF_SCRIPT_ALLOC_ENTRY;
						if (safe_heap_check_address(buffer, string, 1))
						{
							S32 toffset = string;
							safe_heap_bytestream_count_char(buffer, toffset);
							S32 size = toffset - string;
							char *arg = new char[size];
							bytestream2char(arg, buffer, string);
							LLQuaternion quat;
							S32 num = sscanf(arg, "<%f, %f, %f, %f>", &quat.mQ[VX], &quat.mQ[VY], &quat.mQ[VZ], &quat.mQ[VW]);
							if (num != 4)
							{
								quat = LLQuaternion::DEFAULT;

							}
							lscript_push(buffer, quat);
							delete [] arg;
						}
						lsa_decrease_ref_count(buffer, base_address);
					}
				}
				break;
			default:
				break;
			}
		}
		break;
	case LST_KEY:
		{
			switch(to)
			{
			case LST_KEY:
				break;
			case LST_STRING:
				break;
			case LST_LIST:
				{
					S32 saddress = lscript_pop_int(buffer);
					LLScriptLibData *string = lsa_get_data(buffer, saddress, TRUE);
					LLScriptLibData *list = new LLScriptLibData;
					list->mType = LST_LIST;
					list->mListp = string;
					S32 address = lsa_heap_add_data(buffer, list, get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			default:
				break;
			}
		}
		break;
	case LST_VECTOR:
		{
			switch(to)
			{
			case LST_VECTOR:
				break;
			case LST_STRING:
				{
					S32 address;
					LLVector3 source;
					lscript_pop_vector(buffer, source);
					sprintf(caststr, "<%5.5f, %5.5f, %5.5f>", source.mV[VX], source.mV[VY], source.mV[VZ]);
					address = lsa_heap_add_data(buffer, new LLScriptLibData(caststr), get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			case LST_LIST:
				{
					S32 address;
					LLVector3 source;
					lscript_pop_vector(buffer, source);
					LLScriptLibData *list = new LLScriptLibData;
					list->mType = LST_LIST;
					list->mListp = new LLScriptLibData(source);
					address = lsa_heap_add_data(buffer, list, get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			default:
				break;
			}
		}
		break;
	case LST_QUATERNION:
		{
			switch(to)
			{
			case LST_QUATERNION:
				break;
			case LST_STRING:
				{
					S32 address;
					LLQuaternion source;
					lscript_pop_quaternion(buffer, source);
					sprintf(caststr, "<%5.5f, %5.5f, %5.5f, %5.5f>", source.mQ[VX], source.mQ[VY], source.mQ[VZ], source.mQ[VS]);
					address = lsa_heap_add_data(buffer, new LLScriptLibData(caststr), get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			case LST_LIST:
				{
					S32 address;
					LLQuaternion source;
					lscript_pop_quaternion(buffer, source);
					LLScriptLibData *list = new LLScriptLibData;
					list->mType = LST_LIST;
					list->mListp = new LLScriptLibData(source);
					address = lsa_heap_add_data(buffer, list, get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, address);
				}
				break;
			default:
				break;
			}
		}
		break;
	case LST_LIST:
		{
			switch(to)
			{
			case LST_LIST:
				break;
			case LST_STRING:
				{
					S32 address = lscript_pop_int(buffer);
					LLScriptLibData *list = lsa_get_data(buffer, address, TRUE);
					LLScriptLibData *list_root = list;

					std::ostringstream dest;
					while (list)
					{
						list->print(dest, FALSE);
						list = list->mListp;
					}
					delete list_root;
					char *tmp = strdup(dest.str().c_str());
					LLScriptLibData *string = new LLScriptLibData(tmp);
					free(tmp); 
					tmp = NULL;
					S32 destaddress = lsa_heap_add_data(buffer, string, get_max_heap_size(buffer), TRUE);
					lscript_push(buffer, destaddress);
				}
				break;
			default:
				break;
			}
		}
		break;
		default:
			break;
	}
	return FALSE;
}

BOOL run_stacktos(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	S32 length = lscript_pop_int(buffer);
	S32 i;
	char *arg = new char[length];
	S32 fault;
	for (i = 0; i < length; i++)
	{
		fault = get_register(buffer, LREG_FR);
		if (fault)
			break;

		arg[length - i - 1] = lscript_pop_char(buffer);
	}
	S32 address = lsa_heap_add_data(buffer, new LLScriptLibData(arg), get_max_heap_size(buffer), TRUE);
	lscript_push(buffer, address);
	delete [] arg;
	return FALSE;
}

void lscript_stacktol_pop_variable(LLScriptLibData *data, U8 *buffer, char type)
{
	S32 address, string;
	S32 base_address;

	switch(type)
	{
	case LST_INTEGER:
		data->mType = LST_INTEGER;
		data->mInteger = lscript_pop_int(buffer);
		break;
	case LST_FLOATINGPOINT:
		data->mType = LST_FLOATINGPOINT;
		data->mFP = lscript_pop_float(buffer);
		break;
	case LST_KEY:
		data->mType = LST_KEY;
		
		base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
		address = base_address + get_register(buffer, LREG_HR) - 1;

		if (address)
		{
			string = address + SIZEOF_SCRIPT_ALLOC_ENTRY;
			if (safe_heap_check_address(buffer, string, 1))
			{
				S32 toffset = string;
				safe_heap_bytestream_count_char(buffer, toffset);
				S32 size = toffset - string;
				data->mKey = new char[size];
				bytestream2char(data->mKey, buffer, string);
			}
			lsa_decrease_ref_count(buffer, base_address);
		}
		else
		{
			data->mKey = new char[1];
			data->mKey[0] = 0;
		}
		break;
	case LST_STRING:
		data->mType = LST_STRING;

		base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
		address = base_address + get_register(buffer, LREG_HR) - 1;

		if (address)
		{
			string = address + SIZEOF_SCRIPT_ALLOC_ENTRY;
			if (safe_heap_check_address(buffer, string, 1))
			{
				S32 toffset = string;
				safe_heap_bytestream_count_char(buffer, toffset);
				S32 size = toffset - string;
				data->mString = new char[size];
				bytestream2char(data->mString, buffer, string);
			}
			lsa_decrease_ref_count(buffer, base_address);
		}
		else
		{
			data->mString = new char[1];
			data->mString[0] = 0;
		}
		break;
	case LST_VECTOR:
		data->mType = LST_VECTOR;
		lscript_pop_vector(buffer, data->mVec);
		break;
	case LST_QUATERNION:
		data->mType = LST_QUATERNION;
		lscript_pop_quaternion(buffer, data->mQuat);
		break;
	case LST_LIST:
		data->mType = LST_LIST;
		address = lscript_pop_int(buffer);
		data->mListp = lsa_get_data(buffer, address, TRUE);
		break;
	}
}

BOOL run_stacktol(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	offset++;
	S32 length = safe_instruction_bytestream2integer(buffer, offset);
	S32 i;
	S32 fault;

	S8 type;

	LLScriptLibData *data = new LLScriptLibData, *tail;
	data->mType = LST_LIST;

	for (i = 0; i < length; i++)
	{
		fault = get_register(buffer, LREG_FR);
		if (fault)
			break;

		type = lscript_pop_char(buffer);

		tail = new LLScriptLibData;

		lscript_stacktol_pop_variable(tail, buffer, type);

		tail->mListp = data->mListp;
		data->mListp = tail;
	}
	S32 address = lsa_heap_add_data(buffer,data, get_max_heap_size(buffer), TRUE);
	lscript_push(buffer, address);
	return FALSE;
}

BOOL run_print(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tPRINT ", offset);
	offset++;
	U8 type = safe_instruction_bytestream2byte(buffer, offset);
	if (b_print)
	{
		print_type(type);
		printf("\n");
	}
	switch(type)
	{
	case LST_INTEGER:
		{
			S32 source = lscript_pop_int(buffer);
			printf("%d\n", source);
		}
		break;
	case LST_FLOATINGPOINT:
		{
			F32 source = lscript_pop_float(buffer);
			printf("%f\n", source);
		}
		break;
	case LST_STRING:
		{
			S32 base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
			S32	address = base_address + get_register(buffer, LREG_HR) - 1;

			if (address)
			{
				S32 string = address;
				string += SIZEOF_SCRIPT_ALLOC_ENTRY;
				if (safe_heap_check_address(buffer, string, 1))
				{
					S32 toffset = string;
					safe_heap_bytestream_count_char(buffer, toffset);
					S32 size = toffset - string;
					char *arg = new char[size];
					bytestream2char(arg, buffer, string);
					printf("%s\n", arg);
					delete [] arg;
				}
				lsa_decrease_ref_count(buffer, base_address);
			}
		}
		break;
	case LST_VECTOR:
		{
			LLVector3 source;
			lscript_pop_vector(buffer, source);
			printf("< %f, %f, %f >\n", source.mV[VX], source.mV[VY], source.mV[VZ]);
		}
		break;
	case LST_QUATERNION:
		{
			LLQuaternion source;
			lscript_pop_quaternion(buffer, source);
			printf("< %f, %f, %f, %f >\n", source.mQ[VX], source.mQ[VY], source.mQ[VZ], source.mQ[VS]);
		}
		break;
	case LST_LIST:
		{
			S32 base_address = lscript_pop_int(buffer);
			LLScriptLibData *data = lsa_get_data(buffer, base_address, TRUE);
			LLScriptLibData *print = data;

			printf("list\n");

			while (print)
			{
				switch(print->mType)
				{
				case LST_INTEGER:
					{
						printf("%d\n", print->mInteger);
					}
					break;
				case LST_FLOATINGPOINT:
					{
						printf("%f\n", print->mFP);
					}
					break;
				case LST_STRING:
					{
						printf("%s\n", print->mString);
					}
					break;
				case LST_KEY:
					{
						printf("%s\n", print->mKey);
					}
					break;
				case LST_VECTOR:
					{
						printf("< %f, %f, %f >\n", print->mVec.mV[VX], print->mVec.mV[VY], print->mVec.mV[VZ]);
					}
					break;
				case LST_QUATERNION:
					{
						printf("< %f, %f, %f, %f >\n", print->mQuat.mQ[VX], print->mQuat.mQ[VY], print->mQuat.mQ[VZ], print->mQuat.mQ[VS]);
					}
					break;
				default:
					break;
				}
				print = print->mListp;
			}
			delete data;
		}
		break;
	default:
		break;
	}
	return FALSE;
}


void lscript_run(char *filename, BOOL b_debug)
{
	LLTimer	timer;
	char *error;
	BOOL b_state;
	LLScriptExecute *execute = NULL;
	FILE *file = LLFile::fopen(filename, "r");
	if (file)
	{
		execute = new LLScriptExecute(file);
		fclose(file);
	}
	file = LLFile::fopen(filename, "r");
	if (file)
	{
		FILE *fp = LLFile::fopen("lscript.parse", "w");
		LLScriptLSOParse	*parse = new LLScriptLSOParse(file);
		parse->printData(fp);
		fclose(file);
		fclose(fp);
	}
	file = LLFile::fopen(filename, "r");
	if (file && execute)
	{
		timer.reset();
		while (!execute->run(b_debug, LLUUID::null, &error, b_state))
			;
		F32 time = timer.getElapsedTimeF32();
		F32 ips = execute->mInstructionCount / time;
		llinfos << execute->mInstructionCount << " instructions in " << time << " seconds" << llendl;
		llinfos << ips/1000 << "K instructions per second" << llendl;
		printf("ip: 0x%X\n", get_register(execute->mBuffer, LREG_IP));
		printf("sp: 0x%X\n", get_register(execute->mBuffer, LREG_SP));
		printf("bp: 0x%X\n", get_register(execute->mBuffer, LREG_BP));
		printf("hr: 0x%X\n", get_register(execute->mBuffer, LREG_HR));
		printf("hp: 0x%X\n", get_register(execute->mBuffer, LREG_HP));
		delete execute;
		fclose(file);
	}
}

void lscript_pop_variable(LLScriptLibData *data, U8 *buffer, char type)
{
	S32 address, string;
	S32 base_address;

	switch(type)
	{
	case 'i':
		data->mType = LST_INTEGER;
		data->mInteger = lscript_pop_int(buffer);
		break;
	case 'f':
		data->mType = LST_FLOATINGPOINT;
		data->mFP = lscript_pop_float(buffer);
		break;
	case 'k':
		data->mType = LST_KEY;
		
		base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
		address = base_address + get_register(buffer, LREG_HR) - 1;

		if (address)
		{
			string = address + SIZEOF_SCRIPT_ALLOC_ENTRY;
			if (safe_heap_check_address(buffer, string, 1))
			{
				S32 toffset = string;
				safe_heap_bytestream_count_char(buffer, toffset);
				S32 size = toffset - string;
				data->mKey = new char[size];
				bytestream2char(data->mKey, buffer, string);
			}
			lsa_decrease_ref_count(buffer, base_address);
		}
		else
		{
			data->mKey = new char[1];
			data->mKey[0] = 0;
		}
		break;
	case 's':
		data->mType = LST_STRING;

		base_address = lscript_pop_int(buffer);
	// this bit of nastiness is to get around that code paths to local variables can result in lack of initialization
	// and function clean up of ref counts isn't based on scope (a mistake, I know)
		address = base_address + get_register(buffer, LREG_HR) - 1;

		if (address)
		{
			string = address + SIZEOF_SCRIPT_ALLOC_ENTRY;
			if (safe_heap_check_address(buffer, string, 1))
			{
				S32 toffset = string;
				safe_heap_bytestream_count_char(buffer, toffset);
				S32 size = toffset - string;
				data->mString = new char[size];
				bytestream2char(data->mString, buffer, string);
			}
			lsa_decrease_ref_count(buffer, base_address);
		}
		else
		{
			data->mString = new char[1];
			data->mString[0] = 0;
		}
		break;
	case 'l':
		{
			S32 base_address = lscript_pop_int(buffer);
			data->mType = LST_LIST;
			data->mListp = lsa_get_list_ptr(buffer, base_address, TRUE);
		}
		break;
	case 'v':
		data->mType = LST_VECTOR;
		lscript_pop_vector(buffer, data->mVec);
		break;
	case 'q':
		data->mType = LST_QUATERNION;
		lscript_pop_quaternion(buffer, data->mQuat);
		break;
	}
}

void lscript_push_return_variable(LLScriptLibData *data, U8 *buffer)
{
	S32 address;
	switch(data->mType)
	{
	case LST_INTEGER:
		lscript_local_store(buffer, -12, data->mInteger);
		break;
	case LST_FLOATINGPOINT:
		lscript_local_store(buffer, -12, data->mFP);
		break;
	case LST_KEY:
		address = lsa_heap_add_data(buffer, data, get_max_heap_size(buffer), FALSE);
		lscript_local_store(buffer, -12, address);
		break;
	case LST_STRING:
		address = lsa_heap_add_data(buffer, data, get_max_heap_size(buffer), FALSE);
		lscript_local_store(buffer, -12, address);
		break;
	case LST_LIST:
		address = lsa_heap_add_data(buffer, data, get_max_heap_size(buffer), FALSE);
		lscript_local_store(buffer, -12, address);
		break;
	case LST_VECTOR:
		lscript_local_store(buffer, -20, data->mVec);
		break;
	case LST_QUATERNION:
		lscript_local_store(buffer, -24, data->mQuat);
		break;
	default:
		break;
	}
}

S32 lscript_push_variable(LLScriptLibData *data, U8 *buffer)
{
	S32 address;
	switch(data->mType)
	{
	case LST_INTEGER:
		lscript_push(buffer, data->mInteger);
		break;
	case LST_FLOATINGPOINT:
		lscript_push(buffer, data->mFP);
		return 4;
		break;
	case LST_KEY:
		address = lsa_heap_add_data(buffer, data, get_max_heap_size(buffer), FALSE);
		lscript_push(buffer, address);
		return 4;
		break;
	case LST_STRING:
		address = lsa_heap_add_data(buffer, data, get_max_heap_size(buffer), FALSE);
		lscript_push(buffer, address);
		return 4;
		break;
	case LST_LIST:
		address = lsa_heap_add_data(buffer, data, get_max_heap_size(buffer), FALSE);
		lscript_push(buffer, address);
		return 4;
		break;
	case LST_VECTOR:
		lscript_push(buffer, data->mVec);
		return 12;
		break;
	case LST_QUATERNION:
		lscript_push(buffer, data->mQuat);
		return 16;
		break;
	default:
		break;
	}
	return 4;
}

BOOL run_calllib(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tCALLLIB ", offset);
	offset++;
	U8 arg = safe_instruction_bytestream2byte(buffer, offset);
	if (arg >= gScriptLibrary.mNextNumber)
	{
		set_fault(buffer, LSRF_BOUND_CHECK_ERROR);
		return FALSE;
	}
	if (b_print)
		printf("%d (%s)\n", (U32)arg, gScriptLibrary.mFunctions[arg]->mName);

	// pull out the arguments and the return values
	LLScriptLibData	*arguments = NULL;
	LLScriptLibData	*returnvalue = NULL;

	S32 i, number;

	if (gScriptLibrary.mFunctions[arg]->mReturnType)
	{
		returnvalue = new LLScriptLibData;
	}

	if (gScriptLibrary.mFunctions[arg]->mArgs)
	{
		number = (S32)strlen(gScriptLibrary.mFunctions[arg]->mArgs);
		arguments = new LLScriptLibData[number];
	}
	else
	{
		number = 0;
	}

	for (i = number - 1; i >= 0; i--)
	{
		lscript_pop_variable(&arguments[i], buffer, gScriptLibrary.mFunctions[arg]->mArgs[i]);
	}

	if (b_print)
	{
		printf("%s\n", gScriptLibrary.mFunctions[arg]->mDesc);
	}

	{
		// LLFastTimer time_in_libraries1(LLFastTimer::FTM_TEMP7);
		gScriptLibrary.mFunctions[arg]->mExecFunc(returnvalue, arguments, id);
	}
	add_register_fp(buffer, LREG_ESR, -gScriptLibrary.mFunctions[arg]->mEnergyUse);
	add_register_fp(buffer, LREG_SLR, gScriptLibrary.mFunctions[arg]->mSleepTime);

	if (returnvalue)
	{
		returnvalue->mType = char2type(*gScriptLibrary.mFunctions[arg]->mReturnType);
		lscript_push_return_variable(returnvalue, buffer);
	}

	delete [] arguments;
	delete returnvalue;

	// reset the BP after calling the library files
	S32 bp = lscript_pop_int(buffer);
	set_bp(buffer, bp);

	// pop off the spot for the instruction pointer
	lscript_poparg(buffer, 4);
	return FALSE;
}


BOOL run_calllib_two_byte(U8 *buffer, S32 &offset, BOOL b_print, const LLUUID &id)
{
	if (b_print)
		printf("[0x%X]\tCALLLIB ", offset);
	offset++;
	U16 arg = safe_instruction_bytestream2u16(buffer, offset);
	if (arg >= gScriptLibrary.mNextNumber)
	{
		set_fault(buffer, LSRF_BOUND_CHECK_ERROR);
		return FALSE;
	}
	if (b_print)
		printf("%d (%s)\n", (U32)arg, gScriptLibrary.mFunctions[arg]->mName);

	// pull out the arguments and the return values
	LLScriptLibData	*arguments = NULL;
	LLScriptLibData	*returnvalue = NULL;

	S32 i, number;

	if (gScriptLibrary.mFunctions[arg]->mReturnType)
	{
		returnvalue = new LLScriptLibData;
	}

	if (gScriptLibrary.mFunctions[arg]->mArgs)
	{
		number = (S32)strlen(gScriptLibrary.mFunctions[arg]->mArgs);
		arguments = new LLScriptLibData[number];
	}
	else
	{
		number = 0;
	}

	for (i = number - 1; i >= 0; i--)
	{
		lscript_pop_variable(&arguments[i], buffer, gScriptLibrary.mFunctions[arg]->mArgs[i]);
	}

	if (b_print)
	{
		printf("%s\n", gScriptLibrary.mFunctions[arg]->mDesc);
	}

	{
		// LLFastTimer time_in_libraries2(LLFastTimer::FTM_TEMP8);
		gScriptLibrary.mFunctions[arg]->mExecFunc(returnvalue, arguments, id);
	}
	add_register_fp(buffer, LREG_ESR, -gScriptLibrary.mFunctions[arg]->mEnergyUse);
	add_register_fp(buffer, LREG_SLR, gScriptLibrary.mFunctions[arg]->mSleepTime);

	if (returnvalue)
	{
		returnvalue->mType = char2type(*gScriptLibrary.mFunctions[arg]->mReturnType);
		lscript_push_return_variable(returnvalue, buffer);
	}

	delete [] arguments;
	delete returnvalue;

	// reset the BP after calling the library files
	S32 bp = lscript_pop_int(buffer);
	set_bp(buffer, bp);

	// pop off the spot for the instruction pointer
	lscript_poparg(buffer, 4);
	return FALSE;
}
