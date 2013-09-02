#include <iostream>
#include <thread>

// Uncomment to turn on debugging output for this file.
//#define DOUT2
//#define DEBUG_LOOP_PARALLEL
#define DEBUG_THREAD_WRITE

// Hack to get around the fact that it's defined in jsinterp.
#undef dout
#undef dprintf

#ifdef DOUT2
#define dout cout << __FILE__ << "(" << __LINE__ << ") DEBUG: "
#else
#define dout 0 && std::cout
#endif /* DEBUG */

#ifdef DOUT2
#define dprintf(fmt, ...) do { printf(fmt, __VA_ARGS__); } while (0)
#else
#define dprinf(fmt, ...) 0
#endif

using namespace std;

#define PUSH_COPY(v)             do { *regs.sp++ = v; assertSameCompartment(cx, regs.sp[-1]); } while (0)
#define PUSH_COPY_SKIP_CHECK(v)  *regs.sp++ = v
#define PUSH_NULL()              regs.sp++->setNull()
#define PUSH_UNDEFINED()         regs.sp++->setUndefined()
#define PUSH_BOOLEAN(b)          regs.sp++->setBoolean(b)
#define PUSH_DOUBLE(d)           regs.sp++->setDouble(d)
#define PUSH_INT32(i)            regs.sp++->setInt32(i)
#define PUSH_STRING(s)           do { regs.sp++->setString(s); assertSameCompartment(cx, regs.sp[-1]); } while (0)
#define PUSH_OBJECT(obj)         do { regs.sp++->setObject(obj); assertSameCompartment(cx, regs.sp[-1]); } while (0)
#define PUSH_OBJECT_OR_NULL(obj) do { regs.sp++->setObjectOrNull(obj); assertSameCompartment(cx, regs.sp[-1]); } while (0)
#define PUSH_HOLE()              regs.sp++->setMagic(JS_ARRAY_HOLE)
#define POP_COPY_TO(v)           v = *--regs.sp
#define POP_RETURN_VALUE()       regs.fp()->setReturnValue(*--regs.sp)

# define BEGIN_CASE2(OP)     case OP: 
//# define BEGIN_CASE2(OP)     case OP: printf(#OP); printf("\n");
/**
 * CAL The interpret function to be ran in a thread.
 *
 * This is the Interpret function from jsinterp.cpp copy-pasted over and stripped down.  Not every
 * instruction type is supported by this function.  If a non-supported opcode is found, the thread will
 * exit with a message.  (See jsopcode.tbl for a listing of all opcodes.)  The method I was using was I
 * first removed all opcode implementations, execute the desired javascript file, copy in missing opcodes
 * and get it to work.
 *
 * An important note, regs.sp is a pointer in this function while it was not in jsinterp.  You'll need to
 * change the accesses accordingly.
 */
JS_NEVER_INLINE void
ThreadInterpret(int id, jsbytecode* start_pc, JSContext *original_cx, FrameRegs * orig_regs, int offset, jsbytecode *original_pc, jsbytecode *stop_pc, 
        RootedValue *rootValue0, RootedValue *rootValue1, RootedObject *rootObject0, RootedObject *rootObject1, RootedObject *rootObject2, RootedId *rootId0,
        Rooted<JSScript*> * script, int* index, int startP, int stopP, jsid loopIndexID, bool enableWrite)
{
    //JSContext *cx = (JSContext *)malloc(sizeof(JSContext));
    //memcpy(cx, original_cx, sizeof(JSContext));// don't forget the free(cx);
	
	JSContext *cx = original_cx;

	//return;
	//Rooted<JSScript*> script(cx);
	//SET_SCRIPT(regs.fp()->script());
	FrameRegs regs = cx->regs();

    std::set<void *> read;
    std::set<void *> wrote;

	// MainThread exist this bool var
	bool interpReturnOK;
	
    /*
    int threadIndex = startP - 1;
	while(true){
		//(int threadIndex = startP; threadIndex < stopP; threadIndex++) {
		dl_start: //dummy_loop_start
		threadIndex++;
		if(threadIndex >= stopP){
			break;
		}
	*/

	for(int threadIndex = startP; threadIndex < stopP; threadIndex++) {

		int curIndex = index[threadIndex];

		//FrameRegs regs = *orig_regs;
		regs.pc = start_pc;
		// Copy stack.
		Value temp = *(regs.sp);
		regs.sp = &temp;
		//dprintf("[New Thread] ID: %d, Start: %d, Stop: %d\n", id, regs.pc - original_pc, stop_pc - original_pc);
		// dout << "Thread " << id << ", PC: " << regs.pc - original_pc << ", Stop: " << stop_pc - original_pc << endl;

	  #ifdef DEBUG_LOOP_PARALLEL
		printf("Thread[%d], startP=%d, stopP=%d, threadIndex=%d, curIndex=%d, start_pc=%p, sp=%p\n",
				id, startP, stopP, threadIndex, curIndex, (void*)start_pc, (void*)regs.sp);
	  #endif /* DEBUG_LOOP_PARALLEL */

	  #include "interp-defines.h"
		/*
		 * It is important that "op" be initialized before calling DO_OP because
		 * it is possible for "op" to be specially assigned during the normal
		 * processing of an opcode while looping. We rely on DO_NEXT_OP to manage
		 * "op" correctly in all other cases.
		 */
		JSOp op;
		int32_t len=0;
		int switchOp;
		register int switchMask = 0;

		DO_NEXT_OP(len);

		/*
		 * This is a loop, but it does not look like a loop. The loop-closing
		 * jump is distributed throughout goto *jumpTable[op] inside of DO_OP.
		 * When interrupts are enabled, jumpTable is set to interruptJumpTable
		 * where all jumps point to the interrupt label. The latter, after
		 * calling the interrupt handler, dispatches through normalJumpTable to
		 * continue the normal bytecode processing.
		 */

		/* CAL Main interpret loop
		 * Like the above comment says, this doesn't look like it, but it's a loop.
		 * The main loop in fact.  regs.pc += len moves the program counter (len is
		 * set from a table by the operation last executed).  The next line grabs the
		 * opcode and the switch selects the proper action.
		 */
		for (;;) {
		  advance_pc_by_one:
			JS_ASSERT(js_CodeSpec[op].length == 1);
			len = 1;
		  advance_pc:
			js::gc::MaybeVerifyBarriers(cx);
			regs.pc += len; // Set pc (len set by last op to execute)
			offset = regs.pc - original_pc ;
			op = (JSOp) *(regs.pc); // Get the opcode
			//dprintf("[Thread %d] PC: %d  Opcode: %d\n", id, offset, op);
			// dout << "PC:" << offset << " Opcode: " << op << std::endl;

		  do_op:
			if (regs.pc == stop_pc) {
				for (std::set<void *>::iterator i = read.begin(); i != read.end(); ++i)	{
				  #ifdef DEBUG_LOOP_PARALLEL
					printf("[%d] Read %p\n",id, *i);
				  #endif //DEBUG_LOOP_PARALLEL
			    }
				for (std::set<void *>::iterator i = wrote.begin(); i != wrote.end(); ++i) {
				  #ifdef DEBUG_LOOP_PARALLEL
					printf("[%d] Wrote %p\n", id, *i);
				  #endif //DEBUG_LOOP_PARALLEL
				}
				//(*orig_regs) = regs;
			  #ifdef DEBUG_LOOP_PARALLEL
				printf("[DLP][%d] reach the end of loop body: continue dummy loop\n", id);
			  #endif /* DEBUG_LOOP_PARALLEL */

				//Reach the end of loop body
				//goto dl_start;
				//goto dl_end;
				break;
			}

		  #ifdef TRACKPC
			printf("PC:\t%d\n", offset);
		  #endif
			/* BANK */
			CHECK_PCCOUNT_INTERRUPTS_SP();
			switchOp = int(op) | switchMask; // ??
		  //do_switch:
			switch (switchOp) { // CAL Main instruction switch

				/* No-ops for ease of decompilation. */
				ADD_EMPTY_CASE(JSOP_NOP)
				ADD_EMPTY_CASE(JSOP_UNUSED1)
				ADD_EMPTY_CASE(JSOP_UNUSED2)
				ADD_EMPTY_CASE(JSOP_UNUSED3)
				ADD_EMPTY_CASE(JSOP_UNUSED10)
				ADD_EMPTY_CASE(JSOP_UNUSED11)
				ADD_EMPTY_CASE(JSOP_UNUSED12)
				ADD_EMPTY_CASE(JSOP_UNUSED13)
				ADD_EMPTY_CASE(JSOP_UNUSED15)
				ADD_EMPTY_CASE(JSOP_UNUSED17)
				ADD_EMPTY_CASE(JSOP_UNUSED18)
				ADD_EMPTY_CASE(JSOP_UNUSED19)
				ADD_EMPTY_CASE(JSOP_UNUSED20)
				ADD_EMPTY_CASE(JSOP_UNUSED21)
				ADD_EMPTY_CASE(JSOP_UNUSED22)
				ADD_EMPTY_CASE(JSOP_UNUSED23)
				ADD_EMPTY_CASE(JSOP_UNUSED24)
				ADD_EMPTY_CASE(JSOP_UNUSED25)
				ADD_EMPTY_CASE(JSOP_UNUSED29)
				ADD_EMPTY_CASE(JSOP_UNUSED30)
				ADD_EMPTY_CASE(JSOP_UNUSED31)
				ADD_EMPTY_CASE(JSOP_CONDSWITCH)
				ADD_EMPTY_CASE(JSOP_TRY)
				#if JS_HAS_XML_SUPPORT
				ADD_EMPTY_CASE(JSOP_STARTXML)
				ADD_EMPTY_CASE(JSOP_STARTXMLEXPR)
				#endif
				ADD_EMPTY_CASE(JSOP_LOOPHEAD)
				ADD_EMPTY_CASE(JSOP_LOOPENTRY)
				END_EMPTY_CASES

				BEGIN_CASE(JSOP_LABEL)
				END_CASE(JSOP_LABEL)

			  check_backedge:
				{
					CHECK_BRANCH();
					if (op != JSOP_LOOPHEAD)
						DO_OP();

					DO_OP();
				}

				/* ADD_EMPTY_CASE is not used here as JSOP_LINENO_LENGTH == 3. */
				BEGIN_CASE(JSOP_LINENO)
				END_CASE(JSOP_LINENO)

				BEGIN_CASE(JSOP_UNDEFINED)
					PUSH_UNDEFINED();
				END_CASE(JSOP_UNDEFINED)

				BEGIN_CASE(JSOP_POP)
					regs.sp--;
				END_CASE(JSOP_POP)

				BEGIN_CASE(JSOP_POPN)
					JS_ASSERT(GET_UINT16(regs.pc) <= regs.stackDepth());
					regs.sp -= GET_UINT16(regs.pc);
				#ifdef DEBUG
					if (StaticBlockObject *block = regs.fp()->maybeBlockChain())
						JS_ASSERT(regs.stackDepth() >= block->stackDepth() + block->slotCount());
				#endif
				END_CASE(JSOP_POPN)

				BEGIN_CASE(JSOP_SETRVAL)
				BEGIN_CASE(JSOP_POPV)
					POP_RETURN_VALUE();
				END_CASE(JSOP_POPV)

				BEGIN_CASE(JSOP_ENTERWITH)
					if (!EnterWith(cx, -1))
						goto error;

					/*
					 * We must ensure that different "with" blocks have different stack depth
					 * associated with them. This allows the try handler search to properly
					 * recover the scope chain. Thus we must keep the stack at least at the
					 * current level.
					 *
					 * We set sp[-1] to the current "with" object to help asserting the
					 * enter/leave balance in [leavewith].
					 */
					regs.sp[-1].setObject(*regs.fp()->scopeChain());
				END_CASE(JSOP_ENTERWITH)

				BEGIN_CASE(JSOP_LEAVEWITH)
					JS_ASSERT(regs.sp[-1].toObject() == *regs.fp()->scopeChain());
					regs.fp()->popWith(cx);
					regs.sp--;
				END_CASE(JSOP_LEAVEWITH)

				BEGIN_CASE(JSOP_RETURN)
					POP_RETURN_VALUE();
					/* FALL THROUGH */

				BEGIN_CASE(JSOP_RETRVAL)    /* fp return value already set */
				BEGIN_CASE(JSOP_STOP)
				{
					printf("Thread(%d) : JSOP_STOP not Supported", id);
					goto error;
				}

				BEGIN_CASE(JSOP_DEFAULT)
					regs.sp--;
					/* FALL THROUGH */
				BEGIN_CASE(JSOP_GOTO)
				{
					len = GET_JUMP_OFFSET(regs.pc);
					BRANCH(len);
				}
				END_CASE(JSOP_GOTO)

				BEGIN_CASE(JSOP_IFEQ)
				{
					bool cond = ToBoolean(regs.sp[-1]);
					regs.sp--;
					if (cond == false) {
						len = GET_JUMP_OFFSET(regs.pc);
						BRANCH(len);
					}
				}
				END_CASE(JSOP_IFEQ)

				BEGIN_CASE(JSOP_IFNE)
				{
					bool cond = ToBoolean(regs.sp[-1]);
					regs.sp--;
					if (cond != false) {
						len = GET_JUMP_OFFSET(regs.pc);
						BRANCH(len);
					}
				}
				END_CASE(JSOP_IFNE)

				BEGIN_CASE(JSOP_OR)
				{
					bool cond = ToBoolean(regs.sp[-1]);
					if (cond == true) {
						len = GET_JUMP_OFFSET(regs.pc);
						DO_NEXT_OP(len);
					}
				}
				END_CASE(JSOP_OR)

				BEGIN_CASE(JSOP_AND)
				{
					bool cond = ToBoolean(regs.sp[-1]);
					if (cond == false) {
						len = GET_JUMP_OFFSET(regs.pc);
						DO_NEXT_OP(len);
					}
				}
				END_CASE(JSOP_AND)

				/*
				 * If the index value at sp[n] is not an int that fits in a jsval, it could
				 * be an object (an XML QName, AttributeName, or AnyName), but only if we are
				 * compiling with JS_HAS_XML_SUPPORT.  Otherwise convert the index value to a
				 * string atom id.
				 */
				#define FETCH_ELEMENT_ID(obj, n, id)                                          \
					JS_BEGIN_MACRO                                                            \
						const Value &idval_ = regs.sp[n];                                     \
						if (!ValueToId(cx, obj, idval_, id.address()))                        \
							goto error;                                                       \
					JS_END_MACRO

				#define TRY_BRANCH_AFTER_COND(cond,spdec)                                     \
					JS_BEGIN_MACRO                                                            \
						JS_ASSERT(js_CodeSpec[op].length == 1);                               \
						unsigned diff_ = (unsigned) GET_UINT8(regs.pc) - (unsigned) JSOP_IFEQ;         \
						if (diff_ <= 1) {                                                     \
							regs.sp -= spdec;                                                 \
							if (cond == (diff_ != 0)) {                                       \
								++regs.pc;                                                    \
								len = GET_JUMP_OFFSET(regs.pc);                               \
								BRANCH(len);                                                  \
							}                                                                 \
							len = 1 + JSOP_IFEQ_LENGTH;                                       \
							DO_NEXT_OP(len);                                                  \
						}                                                                     \
					JS_END_MACRO

				BEGIN_CASE(JSOP_IN)
				{
					printf("Thread(%d) : JSOP_IN not Supported", id);
					goto error;
				}
				END_CASE(JSOP_IN)

				BEGIN_CASE(JSOP_ITER)
				{
					JS_ASSERT(regs.stackDepth() >= 1);
					uint8_t flags = GET_UINT8(regs.pc);
					MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
					if (!ValueToIterator(cx, flags, res))
						goto error;
					JS_ASSERT(!res.isPrimitive());
				}
				END_CASE(JSOP_ITER)

				BEGIN_CASE(JSOP_MOREITER)
				{
					JS_ASSERT(regs.stackDepth() >= 1);
					JS_ASSERT(regs.sp[-1].isObject());
					PUSH_NULL();
					bool cond;
					MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
					if (!IteratorMore(cx, &regs.sp[-2].toObject(), &cond, res))
						goto error;
					regs.sp[-1].setBoolean(cond);
				}
				END_CASE(JSOP_MOREITER)

				BEGIN_CASE(JSOP_ITERNEXT)
				{
					JS_ASSERT(regs.sp[-1].isObject());
					PUSH_NULL();
					MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
					if (!IteratorNext(cx, &regs.sp[-2].toObject(), res))
						goto error;
				}
				END_CASE(JSOP_ITERNEXT)

				BEGIN_CASE(JSOP_ENDITER)
				{
					JS_ASSERT(regs.stackDepth() >= 1);
					bool ok = CloseIterator(cx, &regs.sp[-1].toObject());
					regs.sp--;
					if (!ok)
						goto error;
				}
				END_CASE(JSOP_ENDITER)

				BEGIN_CASE(JSOP_DUP)
				{
					JS_ASSERT(regs.stackDepth() >= 1);
					const Value &rref = regs.sp[-1];
					PUSH_COPY(rref);
				}
				END_CASE(JSOP_DUP)

				BEGIN_CASE(JSOP_DUP2)
				{
					JS_ASSERT(regs.stackDepth() >= 2);
					const Value &lref = regs.sp[-2];
					const Value &rref = regs.sp[-1];
					PUSH_COPY(lref);
					PUSH_COPY(rref);
				}
				END_CASE(JSOP_DUP2)

				BEGIN_CASE(JSOP_SWAP)
				{
					JS_ASSERT(regs.stackDepth() >= 2);
					Value &lref = regs.sp[-2];
					Value &rref = regs.sp[-1];
					lref.swap(rref);
				}
				END_CASE(JSOP_SWAP)

				BEGIN_CASE(JSOP_PICK)
				{
					unsigned i = GET_UINT8(regs.pc);
					JS_ASSERT(regs.stackDepth() >= i + 1);
					Value lval = regs.sp[-int(i + 1)];
					memmove(regs.sp - (i + 1), regs.sp - i, sizeof(Value) * i);
					regs.sp[-1] = lval;
				}
				END_CASE(JSOP_PICK)

				BEGIN_CASE(JSOP_SETCONST)
				{
					RootedPropertyName &name = rootName0;
					name = script->getName(regs.pc);

					RootedValue &rval = *rootValue0;
					rval = regs.sp[-1];

					RootedObject &obj = *rootObject0;
					obj = &regs.fp()->varObj();
					if (!JSObject::defineProperty(cx, obj, name, rval,
												  JS_PropertyStub, JS_StrictPropertyStub,
												  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
						goto error;
					}
				}
				END_CASE(JSOP_SETCONST);

				#if JS_HAS_DESTRUCTURING
				BEGIN_CASE(JSOP_ENUMCONSTELEM)
				{
					RootedValue &rval = *rootValue0;
					rval = regs.sp[-3];

					RootedObject &obj = *rootObject0;
					FETCH_OBJECT(cx, -2, obj);
					RootedId &id = *rootId0;
					FETCH_ELEMENT_ID(obj, -1, id);
					if (!JSObject::defineGeneric(cx, obj, id, rval,
												 JS_PropertyStub, JS_StrictPropertyStub,
												 JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY)) {
						goto error;
					}
					regs.sp -= 3;
				}
				END_CASE(JSOP_ENUMCONSTELEM)
				#endif

				BEGIN_CASE(JSOP_BINDGNAME)
					PUSH_OBJECT(regs.fp()->global());
				END_CASE(JSOP_BINDGNAME)

				BEGIN_CASE(JSOP_BINDNAME)
				{
					RootedObject &scopeChain = *rootObject0;
					scopeChain = regs.fp()->scopeChain();

					RootedPropertyName &name = rootName0;
					name = script->getName(regs.pc);

					/* Assigning to an undeclared name adds a property to the global object. */
					RootedObject &scope = *rootObject1;
					if (!LookupNameWithGlobalDefault(cx, name, scopeChain, &scope))
						goto error;

					PUSH_OBJECT(*scope);
				}
				END_CASE(JSOP_BINDNAME)

				#define BITWISE_OP(OP)                                                        \
					JS_BEGIN_MACRO                                                            \
						int32_t i, j;                                                         \
						if (!ToInt32(cx, regs.sp[-2], &i))                                    \
							goto error;                                                       \
						if (!ToInt32(cx, regs.sp[-1], &j))                                    \
							goto error;                                                       \
						i = i OP j;                                                           \
						regs.sp--;                                                            \
						regs.sp[-1].setInt32(i);                                              \
					JS_END_MACRO

				BEGIN_CASE(JSOP_BITOR)
					BITWISE_OP(|);
				END_CASE(JSOP_BITOR)

				BEGIN_CASE(JSOP_BITXOR)
					BITWISE_OP(^);
				END_CASE(JSOP_BITXOR)

				BEGIN_CASE(JSOP_BITAND)
					BITWISE_OP(&);
				END_CASE(JSOP_BITAND)

				#undef BITWISE_OP

				#define EQUALITY_OP(OP)                                                       \
					JS_BEGIN_MACRO                                                            \
						Value rval = regs.sp[-1];                                             \
						Value lval = regs.sp[-2];                                             \
						bool cond;                                                            \
						if (!LooselyEqual(cx, lval, rval, &cond))                             \
							goto error;                                                       \
						cond = cond OP JS_TRUE;                                               \
						TRY_BRANCH_AFTER_COND(cond, 2);                                       \
						regs.sp--;                                                            \
						regs.sp[-1].setBoolean(cond);                                         \
					JS_END_MACRO

				BEGIN_CASE(JSOP_EQ)
					EQUALITY_OP(==);
				END_CASE(JSOP_EQ)

				BEGIN_CASE(JSOP_NE)
					EQUALITY_OP(!=);
				END_CASE(JSOP_NE)

				#undef EQUALITY_OP

				#define STRICT_EQUALITY_OP(OP, COND)                                          \
					JS_BEGIN_MACRO                                                            \
						const Value &rref = regs.sp[-1];                                      \
						const Value &lref = regs.sp[-2];                                      \
						bool equal;                                                           \
						if (!StrictlyEqual(cx, lref, rref, &equal))                           \
							goto error;                                                       \
						COND = equal OP JS_TRUE;                                              \
						regs.sp--;                                                            \
					JS_END_MACRO

				BEGIN_CASE(JSOP_STRICTEQ)
				{
					bool cond;
					STRICT_EQUALITY_OP(==, cond);
					regs.sp[-1].setBoolean(cond);
				}
				END_CASE(JSOP_STRICTEQ)

				BEGIN_CASE(JSOP_STRICTNE)
				{
					bool cond;
					STRICT_EQUALITY_OP(!=, cond);
					regs.sp[-1].setBoolean(cond);
				}
				END_CASE(JSOP_STRICTNE)

				BEGIN_CASE(JSOP_CASE)
				{
					bool cond;
					STRICT_EQUALITY_OP(==, cond);
					if (cond) {
						regs.sp--;
						len = GET_JUMP_OFFSET(regs.pc);
						BRANCH(len);
					}
				}
				END_CASE(JSOP_CASE)

				#undef STRICT_EQUALITY_OP

				BEGIN_CASE(JSOP_LT)
				{
					bool cond;
					const Value &lref = regs.sp[-2];
					const Value &rref = regs.sp[-1];
					if (!LessThanOperation(cx, lref, rref, &cond))
						goto error;
					TRY_BRANCH_AFTER_COND(cond, 2);
					regs.sp[-2].setBoolean(cond);
					regs.sp--;
				}
				END_CASE(JSOP_LT)

				BEGIN_CASE(JSOP_LE)
				{
					bool cond;
					const Value &lref = regs.sp[-2];
					const Value &rref = regs.sp[-1];
					if (!LessThanOrEqualOperation(cx, lref, rref, &cond))
						goto error;
					TRY_BRANCH_AFTER_COND(cond, 2);
					regs.sp[-2].setBoolean(cond);
					regs.sp--;
				}
				END_CASE(JSOP_LE)

				BEGIN_CASE(JSOP_GT)
				{
					bool cond;
					const Value &lref = regs.sp[-2];
					const Value &rref = regs.sp[-1];
					if (!GreaterThanOperation(cx, lref, rref, &cond))
						goto error;
					TRY_BRANCH_AFTER_COND(cond, 2);
					regs.sp[-2].setBoolean(cond);
					regs.sp--;
				}
				END_CASE(JSOP_GT)

				BEGIN_CASE(JSOP_GE)
				{
					bool cond;
					const Value &lref = regs.sp[-2];
					const Value &rref = regs.sp[-1];
					if (!GreaterThanOrEqualOperation(cx, lref, rref, &cond))
						goto error;
					TRY_BRANCH_AFTER_COND(cond, 2);
					regs.sp[-2].setBoolean(cond);
					regs.sp--;
				}
				END_CASE(JSOP_GE)

				#define SIGNED_SHIFT_OP(OP)                                                   \
					JS_BEGIN_MACRO                                                            \
						int32_t i, j;                                                         \
						if (!ToInt32(cx, regs.sp[-2], &i))                                    \
							goto error;                                                       \
						if (!ToInt32(cx, regs.sp[-1], &j))                                    \
							goto error;                                                       \
						i = i OP (j & 31);                                                    \
						regs.sp--;                                                            \
						regs.sp[-1].setInt32(i);                                              \
					JS_END_MACRO

				BEGIN_CASE(JSOP_LSH)
					SIGNED_SHIFT_OP(<<);
				END_CASE(JSOP_LSH)

				BEGIN_CASE(JSOP_RSH)
					SIGNED_SHIFT_OP(>>);
				END_CASE(JSOP_RSH)

				#undef SIGNED_SHIFT_OP

				BEGIN_CASE(JSOP_URSH)
				{
					uint32_t u;
					if (!ToUint32(cx, regs.sp[-2], &u))
						goto error;
					int32_t j;
					if (!ToInt32(cx, regs.sp[-1], &j))
						goto error;

					u >>= (j & 31);

					regs.sp--;
					if (!regs.sp[-1].setNumber(uint32_t(u)))
						TypeScript::MonitorOverflow(cx, script, regs.pc);
				}
				END_CASE(JSOP_URSH)

				BEGIN_CASE(JSOP_ADD)
				{
					Value lval = regs.sp[-2];
					Value rval = regs.sp[-1];
					if (!AddOperation(cx, lval, rval, &regs.sp[-2]))
						goto error;
					regs.sp--;
				}
				END_CASE(JSOP_ADD)

				BEGIN_CASE(JSOP_SUB)
				{
					RootedValue &lval = *rootValue0, &rval =*rootValue1;
					lval = regs.sp[-2];
					rval = regs.sp[-1];
					if (!SubOperation(cx, lval, rval, &regs.sp[-2]))
						goto error;
					regs.sp--;
				}
				END_CASE(JSOP_SUB)

				BEGIN_CASE(JSOP_MUL)
				{
					RootedValue &lval = *rootValue0, &rval =*rootValue1;
					lval = regs.sp[-2];
					rval = regs.sp[-1];
					if (!MulOperation(cx, lval, rval, &regs.sp[-2]))
						goto error;
					regs.sp--;
				}
				END_CASE(JSOP_MUL)

				BEGIN_CASE(JSOP_DIV)
				{
					RootedValue &lval = *rootValue0, &rval =*rootValue1;
					lval = regs.sp[-2];
					rval = regs.sp[-1];
					if (!DivOperation(cx, lval, rval, &regs.sp[-2]))
						goto error;
					regs.sp--;
				}
				END_CASE(JSOP_DIV)

				BEGIN_CASE(JSOP_MOD)
				{
					RootedValue &lval = *rootValue0, &rval =*rootValue1;
					lval = regs.sp[-2];
					rval = regs.sp[-1];
					if (!ModOperation(cx, lval, rval, &regs.sp[-2]))
						goto error;
					regs.sp--;
				}
				END_CASE(JSOP_MOD)

				BEGIN_CASE(JSOP_NOT)
				{
					bool cond = ToBoolean(regs.sp[-1]);
					regs.sp--;
					PUSH_BOOLEAN(!cond);
				}
				END_CASE(JSOP_NOT)

				BEGIN_CASE(JSOP_BITNOT)
				{
					int32_t i;
					if (!ToInt32(cx, regs.sp[-1], &i))
						goto error;
					i = ~i;
					regs.sp[-1].setInt32(i);
				}
				END_CASE(JSOP_BITNOT)

				BEGIN_CASE(JSOP_NEG)
				{
					/*
					 * When the operand is int jsval, INT32_FITS_IN_JSVAL(i) implies
					 * INT32_FITS_IN_JSVAL(-i) unless i is 0 or INT32_MIN when the
					 * results, -0.0 or INT32_MAX + 1, are double values.
					 */
					Value ref = regs.sp[-1];
					int32_t i;
					if (ref.isInt32() && (i = ref.toInt32()) != 0 && i != INT32_MIN) {
						i = -i;
						regs.sp[-1].setInt32(i);
					} else {
						double d;
						if (!ToNumber(cx, regs.sp[-1], &d))
							goto error;
						d = -d;
						if (!regs.sp[-1].setNumber(d) && !ref.isDouble())
							TypeScript::MonitorOverflow(cx, script, regs.pc);
					}
				}
				END_CASE(JSOP_NEG)

				BEGIN_CASE(JSOP_POS)
					if (!ToNumber(cx, &regs.sp[-1]))
						goto error;
					if (!regs.sp[-1].isInt32())
						TypeScript::MonitorOverflow(cx, script, regs.pc);
				END_CASE(JSOP_POS)

				BEGIN_CASE(JSOP_DELNAME)
				{
					printf("Thread(%d) : JSOP_DELNAME not Supported", id);
					goto error;
				}
				END_CASE(JSOP_DELNAME)

				BEGIN_CASE(JSOP_DELPROP)
				{
					RootedPropertyName &name = rootName0;
					name = script->getName(regs.pc);

					RootedObject &obj = *rootObject0;
					FETCH_OBJECT(cx, -1, obj);

					MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
					if (!JSObject::deleteProperty(cx, obj, name, res, script->strictModeCode))
						goto error;
				}
				END_CASE(JSOP_DELPROP)

				BEGIN_CASE(JSOP_DELELEM)
				{
					/* Fetch the left part and resolve it to a non-null object. */
					RootedObject &obj = *rootObject0;
					FETCH_OBJECT(cx, -2, obj);

					RootedValue &propval = *rootValue0;
					propval = regs.sp[-1];

					MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
					if (!JSObject::deleteByValue(cx, obj, propval, res, script->strictModeCode))
						goto error;

					regs.sp--;
				}
				END_CASE(JSOP_DELELEM)

				BEGIN_CASE(JSOP_TOID)
				{
					/*
					 * Increment or decrement requires use to lookup the same property twice, but we need to avoid
					 * the oberservable stringification the second time.
					 * There must be an object value below the id, which will not be popped
					 * but is necessary in interning the id for XML.
					 */
					RootedValue &objval = *rootValue0, &idval =*rootValue1;
					objval = regs.sp[-2];
					idval = regs.sp[-1];

					MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-1]);
					if (!ToIdOperation(cx, objval, idval, res))
						goto error;
				}
				END_CASE(JSOP_TOID)

				BEGIN_CASE(JSOP_TYPEOFEXPR)
				BEGIN_CASE(JSOP_TYPEOF)
				{
					const Value &ref = regs.sp[-1];
					JSType type = JS_TypeOfValue(cx, ref);
					regs.sp[-1].setString(rt->atomState.typeAtoms[type]);
				}
				END_CASE(JSOP_TYPEOF)

				BEGIN_CASE(JSOP_VOID)
					regs.sp[-1].setUndefined();
				END_CASE(JSOP_VOID)

				BEGIN_CASE(JSOP_INCELEM)
				BEGIN_CASE(JSOP_DECELEM)
				BEGIN_CASE(JSOP_ELEMINC)
				BEGIN_CASE(JSOP_ELEMDEC)
					/* No-op */
				END_CASE(JSOP_INCELEM)

				BEGIN_CASE(JSOP_INCPROP)
				BEGIN_CASE(JSOP_DECPROP)
				BEGIN_CASE(JSOP_PROPINC)
				BEGIN_CASE(JSOP_PROPDEC)
				BEGIN_CASE(JSOP_INCNAME)
				BEGIN_CASE(JSOP_DECNAME)
				BEGIN_CASE(JSOP_NAMEINC)
				BEGIN_CASE(JSOP_NAMEDEC)
				BEGIN_CASE(JSOP_INCGNAME)
				BEGIN_CASE(JSOP_DECGNAME)
				BEGIN_CASE(JSOP_GNAMEINC)
				BEGIN_CASE(JSOP_GNAMEDEC)
					/* No-op */
				END_CASE(JSOP_INCPROP)

				BEGIN_CASE(JSOP_DECALIASEDVAR)
				BEGIN_CASE(JSOP_ALIASEDVARDEC)
				BEGIN_CASE(JSOP_INCALIASEDVAR)
				BEGIN_CASE(JSOP_ALIASEDVARINC)
					/* No-op */
				END_CASE(JSOP_ALIASEDVARINC)

				BEGIN_CASE(JSOP_DECARG)
				BEGIN_CASE(JSOP_ARGDEC)
				BEGIN_CASE(JSOP_INCARG)
				BEGIN_CASE(JSOP_ARGINC)
				{
					unsigned i = GET_ARGNO(regs.pc);
					if (script->argsObjAliasesFormals()) {
						const Value &arg = regs.fp()->argsObj().arg(i);
						Value v;
						if (!DoIncDec(cx, script, regs.pc, arg, &v, &regs.sp[0]))
							goto error;
						regs.fp()->argsObj().setArg(i, v);
					} else {
						Value &arg = regs.fp()->unaliasedFormal(i);
						if (!DoIncDec(cx, script, regs.pc, arg, &arg, &regs.sp[0]))
							goto error;
					}
					regs.sp++;
				}
				END_CASE(JSOP_ARGINC);

				BEGIN_CASE(JSOP_DECLOCAL)
				BEGIN_CASE(JSOP_LOCALDEC)
				BEGIN_CASE(JSOP_INCLOCAL)
				BEGIN_CASE(JSOP_LOCALINC)
				{
					unsigned i = GET_SLOTNO(regs.pc);
					Value &local = regs.fp()->unaliasedLocal(i);
					if (!DoIncDec(cx, script, regs.pc, local, &local, &regs.sp[0]))
						goto error;
					regs.sp++;
				}
				END_CASE(JSOP_LOCALINC)

				BEGIN_CASE(JSOP_THIS)
					if (!ComputeThis(cx, regs.fp()))
						goto error;
					PUSH_COPY(regs.fp()->thisValue());
				END_CASE(JSOP_THIS)

				BEGIN_CASE(JSOP_GETPROP)
				BEGIN_CASE(JSOP_GETXPROP)
				BEGIN_CASE(JSOP_LENGTH)
				BEGIN_CASE(JSOP_CALLPROP)
				{
					RootedValue &lval = *rootValue0;
					lval = regs.sp[-1];

					RootedValue rval(cx);
					if (!GetPropertyOperation(cx, regs.pc, &lval, &rval))
						goto error;

					TypeScript::Monitor(cx, script, regs.pc, rval);

					regs.sp[-1] = rval;
					assertSameCompartment(cx, regs.sp[-1]);
				}
				END_CASE(JSOP_GETPROP)

				BEGIN_CASE(JSOP_SETGNAME)
				BEGIN_CASE(JSOP_SETNAME)
				{
					RootedObject &scope = *rootObject0;
					scope = &regs.sp[-2].toObject();

					HandleValue value = HandleValue::fromMarkedLocation(&regs.sp[-1]);

					if (!SetNameOperation(cx, script, regs.pc, scope, value))
						goto error;

					regs.sp[-2] = regs.sp[-1];
					regs.sp--;
				}
				END_CASE(JSOP_SETNAME)

				BEGIN_CASE(JSOP_SETPROP)
				{
					HandleValue lval = HandleValue::fromMarkedLocation(&regs.sp[-2]);
					HandleValue rval = HandleValue::fromMarkedLocation(&regs.sp[-1]);

					if (!SetPropertyOperation(cx, regs.pc, lval, rval))
						goto error;

					regs.sp[-2] = regs.sp[-1];
					regs.sp--;
				}
				END_CASE(JSOP_SETPROP)

				BEGIN_CASE(JSOP_GETELEM)
				BEGIN_CASE(JSOP_CALLELEM)
				{
					MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
					HandleValue rval = HandleValue::fromMarkedLocation(&regs.sp[-1]);

					MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
					if (!GetElementOperation(cx, op, lval, rval, res))
						goto error;
					TypeScript::Monitor(cx, script, regs.pc, res);
					regs.sp--;
				}
				END_CASE(JSOP_GETELEM)

				BEGIN_CASE(JSOP_SETELEM)
				{
					RootedObject &obj = *rootObject0;
					FETCH_OBJECT(cx, -3, obj);
					RootedId &id = *rootId0;
					FETCH_ELEMENT_ID(obj, -2, id);
					Value &value = regs.sp[-1];
					if (!SetObjectElementOperation(cx, obj, id, value, script->strictModeCode))
						goto error;
					regs.sp[-3] = value;
					regs.sp -= 2;
				}
				END_CASE(JSOP_SETELEM)

				BEGIN_CASE(JSOP_ENUMELEM)
				{
					RootedObject &obj = *rootObject0;
					RootedValue &rval = *rootValue0;

					/* Funky: the value to set is under the [obj, id] pair. */
					FETCH_OBJECT(cx, -2, obj);
					RootedId &id = *rootId0;
					FETCH_ELEMENT_ID(obj, -1, id);
					rval = regs.sp[-3];
					if (!JSObject::setGeneric(cx, obj, obj, id, &rval, script->strictModeCode))
						goto error;
					regs.sp -= 3;
				}
				END_CASE(JSOP_ENUMELEM)

				BEGIN_CASE(JSOP_EVAL)
				{
					CallArgs args = CallArgsFromSp(GET_ARGC(regs.pc), regs.sp);
					if (IsBuiltinEvalForScope(regs.fp()->scopeChain(), args.calleev())) {
						if (!DirectEval(cx, args))
							goto error;
					} else {
						if (!InvokeKernel(cx, args))
							goto error;
					}
					regs.sp = args.spAfterCall();
					TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
				}
				END_CASE(JSOP_EVAL)

				BEGIN_CASE(JSOP_FUNAPPLY)
					if (!GuardFunApplyArgumentsOptimization(cx))
						goto error;
					/* FALL THROUGH */

				BEGIN_CASE2(JSOP_NEW)
				BEGIN_CASE2(JSOP_CALL)
				BEGIN_CASE2(JSOP_FUNCALL)
				{
					std::cout << "Functions not supported in loop. " << std::endl << "Opcode: " << op  << " PC: " << regs.pc - original_pc << std::endl;
					goto error;
				}
				END_CASE(JSOP_NEW)

				BEGIN_CASE(JSOP_SETCALL)
				{
					JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_LEFTSIDE_OF_ASS);
					goto error;
				}
				END_CASE(JSOP_SETCALL)

				BEGIN_CASE(JSOP_IMPLICITTHIS)
				{
					RootedPropertyName &name = rootName0;
					name = script->getName(regs.pc);

					RootedObject &scopeObj = *rootObject0;
					scopeObj = cx->stack.currentScriptedScopeChain();

					RootedObject &scope = *rootObject1;
					if (!LookupNameWithGlobalDefault(cx, name, scopeObj, &scope))
						goto error;

					Value v;
					if (!ComputeImplicitThis(cx, scope, &v))
						goto error;
					PUSH_COPY(v);
				}
				END_CASE(JSOP_IMPLICITTHIS)

				BEGIN_CASE(JSOP_GETGNAME)
				BEGIN_CASE(JSOP_CALLGNAME)
				BEGIN_CASE(JSOP_NAME)
				BEGIN_CASE(JSOP_CALLNAME)
				{
					RootedValue &rval = *rootValue0;

					if (!NameOperation(cx, script, regs.pc, rval.address()))
						goto error;

					PUSH_COPY(rval);
					TypeScript::Monitor(cx, script, regs.pc, rval);
				}
				END_CASE(JSOP_NAME)

				BEGIN_CASE(JSOP_INTRINSICNAME)
				BEGIN_CASE(JSOP_CALLINTRINSIC)
				{
					RootedValue &rval = *rootValue0;

					if (!IntrinsicNameOperation(cx, script, regs.pc, rval.address()))
						goto error;

					PUSH_COPY(rval);
					TypeScript::Monitor(cx, script, regs.pc, rval);
				}
				END_CASE(JSOP_INTRINSICNAME)

				BEGIN_CASE(JSOP_UINT16)
					PUSH_INT32((int32_t) GET_UINT16(regs.pc));
				END_CASE(JSOP_UINT16)

				BEGIN_CASE(JSOP_UINT24)
					PUSH_INT32((int32_t) GET_UINT24(regs.pc));
				END_CASE(JSOP_UINT24)

				BEGIN_CASE(JSOP_INT8)
					PUSH_INT32(GET_INT8(regs.pc));
				END_CASE(JSOP_INT8)

				BEGIN_CASE(JSOP_INT32)
					PUSH_INT32(GET_INT32(regs.pc));
				END_CASE(JSOP_INT32)

				BEGIN_CASE(JSOP_DOUBLE)
				{
					double dbl;
					LOAD_DOUBLE(0, dbl);
					PUSH_DOUBLE(dbl);
				}
				END_CASE(JSOP_DOUBLE)

				BEGIN_CASE(JSOP_STRING)
					PUSH_STRING(script->getAtom(regs.pc));
				END_CASE(JSOP_STRING)

				BEGIN_CASE(JSOP_OBJECT)
					PUSH_OBJECT(*script->getObject(regs.pc));
				END_CASE(JSOP_OBJECT)

				BEGIN_CASE(JSOP_REGEXP)
				{
					/*
					 * Push a regexp object cloned from the regexp literal object mapped by the
					 * bytecode at pc.
					 */
					uint32_t index = GET_UINT32_INDEX(regs.pc);
					JSObject *proto = regs.fp()->global().getOrCreateRegExpPrototype(cx);
					if (!proto)
						goto error;
					JSObject *obj = CloneRegExpObject(cx, script->getRegExp(index), proto);
					if (!obj)
						goto error;
					PUSH_OBJECT(*obj);
				}
				END_CASE(JSOP_REGEXP)

				BEGIN_CASE(JSOP_ZERO)
					PUSH_INT32(0);
				END_CASE(JSOP_ZERO)

				BEGIN_CASE(JSOP_ONE)
					PUSH_INT32(1);
				END_CASE(JSOP_ONE)

				BEGIN_CASE(JSOP_NULL)
					PUSH_NULL();
				END_CASE(JSOP_NULL)

				BEGIN_CASE(JSOP_FALSE)
					PUSH_BOOLEAN(false);
				END_CASE(JSOP_FALSE)

				BEGIN_CASE(JSOP_TRUE)
					PUSH_BOOLEAN(true);
				END_CASE(JSOP_TRUE)

				{
				BEGIN_CASE(JSOP_TABLESWITCH)
				{
					jsbytecode *pc2 = regs.pc;
					len = GET_JUMP_OFFSET(pc2);

					/*
					 * ECMAv2+ forbids conversion of discriminant, so we will skip to the
					 * default case if the discriminant isn't already an int jsval.  (This
					 * opcode is emitted only for dense int-domain switches.)
					 */
					const Value &rref = *--regs.sp;
					int32_t i;
					if (rref.isInt32()) {
						i = rref.toInt32();
					} else {
						double d;
						/* Don't use MOZ_DOUBLE_IS_INT32; treat -0 (double) as 0. */
						if (!rref.isDouble() || (d = rref.toDouble()) != (i = int32_t(rref.toDouble())))
							DO_NEXT_OP(len);
					}

					pc2 += JUMP_OFFSET_LEN;
					int32_t low = GET_JUMP_OFFSET(pc2);
					pc2 += JUMP_OFFSET_LEN;
					int32_t high = GET_JUMP_OFFSET(pc2);

					i -= low;
					if ((uint32_t)i < (uint32_t)(high - low + 1)) {
						pc2 += JUMP_OFFSET_LEN + JUMP_OFFSET_LEN * i;
						int32_t off = (int32_t) GET_JUMP_OFFSET(pc2);
						if (off)
							len = off;
					}
				}
				END_VARLEN_CASE
				}

				{
				BEGIN_CASE(JSOP_LOOKUPSWITCH)
				{
					int32_t off;
					off = JUMP_OFFSET_LEN;

					/*
					 * JSOP_LOOKUPSWITCH are never used if any atom index in it would exceed
					 * 64K limit.
					 */
					jsbytecode *pc2 = regs.pc;

					Value lval = regs.sp[-1];
					regs.sp--;

					int npairs;
					if (!lval.isPrimitive())
						goto end_lookup_switch;

					pc2 += off;
					npairs = GET_UINT16(pc2);
					pc2 += UINT16_LEN;
					JS_ASSERT(npairs);  /* empty switch uses JSOP_TABLESWITCH */

					bool match;
				#define SEARCH_PAIRS(MATCH_CODE)                                              \
					for (;;) {                                                                \
						Value rval = script->getConst(GET_UINT32_INDEX(pc2));                 \
						MATCH_CODE                                                            \
						pc2 += UINT32_INDEX_LEN;                                              \
						if (match)                                                            \
							break;                                                            \
						pc2 += off;                                                           \
						if (--npairs == 0) {                                                  \
							pc2 = regs.pc;                                                    \
							break;                                                            \
						}                                                                     \
					}

					if (lval.isString()) {
						JSLinearString *str = lval.toString()->ensureLinear(cx);
						if (!str)
							goto error;
						JSLinearString *str2;
						SEARCH_PAIRS(
							match = (rval.isString() &&
									 ((str2 = &rval.toString()->asLinear()) == str ||
									  EqualStrings(str2, str)));
						)
					} else if (lval.isNumber()) {
						double ldbl = lval.toNumber();
						SEARCH_PAIRS(
							match = rval.isNumber() && ldbl == rval.toNumber();
						)
					} else {
						SEARCH_PAIRS(
							match = (lval == rval);
						)
					}
				#undef SEARCH_PAIRS

				  end_lookup_switch:
					len = GET_JUMP_OFFSET(pc2);
				}
				END_VARLEN_CASE
				}

				BEGIN_CASE(JSOP_ACTUALSFILLED)
				{
					PUSH_INT32(Max(regs.fp()->numActualArgs(), GET_UINT16(regs.pc)));
				}
				END_CASE(JSOP_ACTUALSFILLED)

				BEGIN_CASE(JSOP_ARGUMENTS)
					JS_ASSERT(!regs.fp()->fun()->hasRest());
					if (script->needsArgsObj()) {
						ArgumentsObject *obj = ArgumentsObject::createExpected(cx, regs.fp());
						if (!obj)
							goto error;
						PUSH_COPY(ObjectValue(*obj));
					} else {
						PUSH_COPY(MagicValue(JS_OPTIMIZED_ARGUMENTS));
					}
				END_CASE(JSOP_ARGUMENTS)

				BEGIN_CASE(JSOP_REST)
				{
					RootedObject &rest = *rootObject0;
					rest = regs.fp()->createRestParameter(cx);
					if (!rest)
						goto error;
					PUSH_COPY(ObjectValue(*rest));
					if (!SetInitializerObjectType(cx, script, regs.pc, rest))
						goto error;
				}
				END_CASE(JSOP_REST)

				BEGIN_CASE(JSOP_CALLALIASEDVAR)
				BEGIN_CASE(JSOP_GETALIASEDVAR)
				{
					ScopeCoordinate sc = ScopeCoordinate(regs.pc);
					PUSH_COPY(regs.fp()->aliasedVarScope(sc).aliasedVar(sc));
					TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
				}
				END_CASE(JSOP_GETALIASEDVAR)

				BEGIN_CASE(JSOP_SETALIASEDVAR)
				{
					ScopeCoordinate sc = ScopeCoordinate(regs.pc);
					regs.fp()->aliasedVarScope(sc).setAliasedVar(sc, regs.sp[-1]);
				}
				END_CASE(JSOP_SETALIASEDVAR)

				BEGIN_CASE(JSOP_GETARG)
				BEGIN_CASE(JSOP_CALLARG)
				{
					unsigned i = GET_ARGNO(regs.pc);
					if (script->argsObjAliasesFormals())
						PUSH_COPY(regs.fp()->argsObj().arg(i));
					else
						PUSH_COPY(regs.fp()->unaliasedFormal(i));
				}
				END_CASE(JSOP_GETARG)

				BEGIN_CASE(JSOP_SETARG)
				{
					unsigned i = GET_ARGNO(regs.pc);
					if (script->argsObjAliasesFormals())
						regs.fp()->argsObj().setArg(i, regs.sp[-1]);
					else
						regs.fp()->unaliasedFormal(i) = regs.sp[-1];
				}
				END_CASE(JSOP_SETARG)

				BEGIN_CASE(JSOP_GETLOCAL)
				BEGIN_CASE(JSOP_CALLLOCAL)
				{
					unsigned i = GET_SLOTNO(regs.pc);
					PUSH_COPY_SKIP_CHECK(regs.fp()->unaliasedLocal(i));

					/*
					 * Skip the same-compartment assertion if the local will be immediately
					 * popped. We do not guarantee sync for dead locals when coming in from the
					 * method JIT, and a GETLOCAL followed by POP is not considered to be
					 * a use of the variable.
					 */
					if (regs.pc[JSOP_GETLOCAL_LENGTH] != JSOP_POP)
						assertSameCompartment(cx, regs.sp[-1]);
				}
				END_CASE(JSOP_GETLOCAL)

				BEGIN_CASE(JSOP_SETLOCAL)
				{
					unsigned i = GET_SLOTNO(regs.pc);
					regs.fp()->unaliasedLocal(i) = regs.sp[-1];
				}
				END_CASE(JSOP_SETLOCAL)

				BEGIN_CASE(JSOP_DEFCONST)
				BEGIN_CASE(JSOP_DEFVAR)
				{
					/* ES5 10.5 step 8 (with subsequent errata). */
					unsigned attrs = JSPROP_ENUMERATE;
					if (!regs.fp()->isEvalFrame())
						attrs |= JSPROP_PERMANENT;
					if (op == JSOP_DEFCONST)
						attrs |= JSPROP_READONLY;

					/* Step 8b. */
					RootedObject &obj = *rootObject0;
					obj = &regs.fp()->varObj();

					RootedPropertyName &name = rootName0;
					name = script->getName(regs.pc);

					if (!DefVarOrConstOperation(cx, obj, name, attrs))
						goto error;
				}
				END_CASE(JSOP_DEFVAR)

				BEGIN_CASE(JSOP_DEFFUN)
				{
					printf("Thread(%d) : JSOP_DEFFUN not Supported", id);
					goto error;
				}
				END_CASE(JSOP_DEFFUN)

				BEGIN_CASE(JSOP_LAMBDA)
				{
					/* Load the specified function object literal. */
					RootedFunction &fun = rootFunction0;
					fun = script->getFunction(GET_UINT32_INDEX(regs.pc));

					JSFunction *obj = CloneFunctionObjectIfNotSingleton(cx, fun, regs.fp()->scopeChain());
					if (!obj)
						goto error;

					JS_ASSERT(obj->getProto());
					PUSH_OBJECT(*obj);
				}
				END_CASE(JSOP_LAMBDA)

				BEGIN_CASE(JSOP_CALLEE)
					JS_ASSERT(regs.fp()->isNonEvalFunctionFrame());
					PUSH_COPY(regs.fp()->calleev());
				END_CASE(JSOP_CALLEE)

				BEGIN_CASE(JSOP_GETTER)
				BEGIN_CASE(JSOP_SETTER)
				{
					JSOp op2 = JSOp(*++regs.pc);
					RootedId &id = *rootId0;
					RootedValue &rval = *rootValue0;
					RootedValue &scratch =*rootValue1;
					int i;

					RootedObject &obj = *rootObject0;
					switch (op2) {
					  case JSOP_SETNAME:
					  case JSOP_SETPROP:
						id = NameToId(script->getName(regs.pc));
						rval = regs.sp[-1];
						i = -1;
						goto gs_pop_lval;
					  case JSOP_SETELEM:
						rval = regs.sp[-1];
						id = JSID_VOID;
						i = -2;
					  gs_pop_lval:
						FETCH_OBJECT(cx, i - 1, obj);
						break;

					  case JSOP_INITPROP:
						JS_ASSERT(regs.stackDepth() >= 2);
						rval = regs.sp[-1];
						i = -1;
						id = NameToId(script->getName(regs.pc));
						goto gs_get_lval;
					  default:
						JS_ASSERT(op2 == JSOP_INITELEM);
						JS_ASSERT(regs.stackDepth() >= 3);
						rval = regs.sp[-1];
						id = JSID_VOID;
						i = -2;
					  gs_get_lval:
					  {
						const Value &lref = regs.sp[i-1];
						JS_ASSERT(lref.isObject());
						obj = &lref.toObject();
						break;
					  }
					}

					/* Ensure that id has a type suitable for use with obj. */
					if (JSID_IS_VOID(id))
						FETCH_ELEMENT_ID(obj, i, id);

					if (!js_IsCallable(rval)) {
						JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_GETTER_OR_SETTER,
											 (op == JSOP_GETTER) ? js_getter_str : js_setter_str);
						goto error;
					}

					/*
					 * Getters and setters are just like watchpoints from an access control
					 * point of view.
					 */
					Value rtmp;
					unsigned attrs;
					if (!CheckAccess(cx, obj, id, JSACC_WATCH, &rtmp, &attrs))
						goto error;

					PropertyOp getter;
					StrictPropertyOp setter;
					if (op == JSOP_GETTER) {
						getter = CastAsPropertyOp(&rval.toObject());
						setter = JS_StrictPropertyStub;
						attrs = JSPROP_GETTER;
					} else {
						getter = JS_PropertyStub;
						setter = CastAsStrictPropertyOp(&rval.toObject());
						attrs = JSPROP_SETTER;
					}
					attrs |= JSPROP_ENUMERATE | JSPROP_SHARED;

					scratch.setUndefined();
					if (!JSObject::defineGeneric(cx, obj, id, scratch, getter, setter, attrs))
						goto error;

					regs.sp += i;
					if (js_CodeSpec[op2].ndefs > js_CodeSpec[op2].nuses) {
						JS_ASSERT(js_CodeSpec[op2].ndefs == js_CodeSpec[op2].nuses + 1);
						regs.sp[-1] = rval;
						assertSameCompartment(cx, regs.sp[-1]);
					}
					len = js_CodeSpec[op2].length;
					DO_NEXT_OP(len);
				}

				BEGIN_CASE(JSOP_HOLE)
					PUSH_HOLE();
				END_CASE(JSOP_HOLE)

				BEGIN_CASE(JSOP_NEWINIT)
				{
					uint8_t i = GET_UINT8(regs.pc);
					JS_ASSERT(i == JSProto_Array || i == JSProto_Object);

					RootedObject &obj = *rootObject0;
					if (i == JSProto_Array) {
						obj = NewDenseEmptyArray(cx);
					} else {
						gc::AllocKind kind = GuessObjectGCKind(0);
						obj = NewBuiltinClassInstance(cx, &ObjectClass, kind);
					}
					if (!obj || !SetInitializerObjectType(cx, script, regs.pc, obj))
						goto error;

					PUSH_OBJECT(*obj);
					TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
				}
				END_CASE(JSOP_NEWINIT)

				BEGIN_CASE(JSOP_NEWARRAY)
				{
					unsigned count = GET_UINT24(regs.pc);
					RootedObject &obj = *rootObject0;
					obj = NewDenseAllocatedArray(cx, count);
					if (!obj || !SetInitializerObjectType(cx, script, regs.pc, obj))
						goto error;

					PUSH_OBJECT(*obj);
					TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
				}
				END_CASE(JSOP_NEWARRAY)

				BEGIN_CASE(JSOP_NEWOBJECT)
				{
					RootedObject &baseobj = *rootObject0;
					baseobj = script->getObject(regs.pc);

					RootedObject &obj = *rootObject1;
					obj = CopyInitializerObject(cx, baseobj);
					if (!obj || !SetInitializerObjectType(cx, script, regs.pc, obj))
						goto error;

					PUSH_OBJECT(*obj);
					TypeScript::Monitor(cx, script, regs.pc, regs.sp[-1]);
				}
				END_CASE(JSOP_NEWOBJECT)

				BEGIN_CASE(JSOP_ENDINIT)
				{
					/* FIXME remove JSOP_ENDINIT bug 588522 */
					JS_ASSERT(regs.stackDepth() >= 1);
					JS_ASSERT(regs.sp[-1].isObject());
				}
				END_CASE(JSOP_ENDINIT)

				BEGIN_CASE(JSOP_INITPROP)
				{
					/* Load the property's initial value into rval. */
					JS_ASSERT(regs.stackDepth() >= 2);
					RootedValue &rval = *rootValue0;
					rval = regs.sp[-1];

					/* Load the object being initialized into lval/obj. */
					RootedObject &obj = *rootObject0;
					obj = &regs.sp[-2].toObject();
					JS_ASSERT(obj->isObject());

					PropertyName *name = script->getName(regs.pc);

					RootedId &id = *rootId0;
					id = NameToId(name);

					if (JS_UNLIKELY(name == cx->runtime->atomState.protoAtom)
						? !baseops::SetPropertyHelper(cx, obj, obj, id, 0, &rval, script->strictModeCode)
						: !DefineNativeProperty(cx, obj, id, rval, NULL, NULL,
												JSPROP_ENUMERATE, 0, 0, 0)) {
						goto error;
					}

					regs.sp--;
				}
				END_CASE(JSOP_INITPROP);

				BEGIN_CASE(JSOP_INITELEM_INC)
				BEGIN_CASE(JSOP_INITELEM)
				{
					/* Pop the element's value into rval. */
					JS_ASSERT(regs.stackDepth() >= 3);
					HandleValue rref = HandleValue::fromMarkedLocation(&regs.sp[-1]);

					RootedObject &obj = *rootObject0;

					/* Find the object being initialized at top of stack. */
					const Value &lref = regs.sp[-3];
					JS_ASSERT(lref.isObject());
					obj = &lref.toObject();

					/* Fetch id now that we have obj. */
					RootedId &id = *rootId0;
					FETCH_ELEMENT_ID(obj, -2, id);

					/*
					 * If rref is a hole, do not call JSObject::defineProperty. In this case,
					 * obj must be an array, so if the current op is the last element
					 * initialiser, set the array length to one greater than id.
					 */
					if (rref.isMagic(JS_ARRAY_HOLE)) {
						JS_ASSERT(obj->isArray());
						JS_ASSERT(JSID_IS_INT(id));
						JS_ASSERT(uint32_t(JSID_TO_INT(id)) < StackSpace::ARGS_LENGTH_MAX);
						if (JSOp(regs.pc[JSOP_INITELEM_LENGTH]) == JSOP_ENDINIT &&
							!SetLengthProperty(cx, obj, (uint32_t) (JSID_TO_INT(id) + 1)))
						{
							goto error;
						}
					} else {
						if (!JSObject::defineGeneric(cx, obj, id, rref, NULL, NULL, JSPROP_ENUMERATE))
							goto error;
					}
					if (op == JSOP_INITELEM_INC) {
						JS_ASSERT(obj->isArray());
						if (JSID_TO_INT(id) == INT32_MAX) {
							JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
												 JSMSG_SPREAD_TOO_LARGE);
							goto error;
						}
						regs.sp[-2].setInt32(JSID_TO_INT(id) + 1);
						regs.sp--;
					} else {
						regs.sp -= 2;
					}
				}
				END_CASE(JSOP_INITELEM)

				BEGIN_CASE(JSOP_SPREAD)
				{
					int32_t count = regs.sp[-2].toInt32();
					RootedObject arr(cx, &regs.sp[-3].toObject());
					const Value iterable = regs.sp[-1];
					ForOfIterator iter(cx, iterable);
					RootedValue &iterVal = *rootValue0;
					while (iter.next()) {
						if (count == INT32_MAX) {
							JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
												 JSMSG_SPREAD_TOO_LARGE);
							goto error;
						}
						iterVal = iter.value();
						if (!JSObject::defineElement(cx, arr, count++, iterVal, NULL, NULL, JSPROP_ENUMERATE))
							goto error;
					}
					if (!iter.close())
						goto error;
					regs.sp[-2].setInt32(count);
					regs.sp--;
				}
				END_CASE(JSOP_SPREAD)

				{
				BEGIN_CASE(JSOP_GOSUB)
					PUSH_BOOLEAN(false);
					int32_t i = (regs.pc - script->code) + JSOP_GOSUB_LENGTH;
					len = GET_JUMP_OFFSET(regs.pc);
					PUSH_INT32(i);
				END_VARLEN_CASE
				}

				{
				BEGIN_CASE(JSOP_RETSUB)
					/* Pop [exception or hole, retsub pc-index]. */
					Value rval, lval;
					POP_COPY_TO(rval);
					POP_COPY_TO(lval);
					JS_ASSERT(lval.isBoolean());
					if (lval.toBoolean()) {
						/*
						 * Exception was pending during finally, throw it *before* we adjust
						 * pc, because pc indexes into script->trynotes.  This turns out not to
						 * be necessary, but it seems clearer.  And it points out a FIXME:
						 * 350509, due to Igor Bukanov.
						 */
						cx->setPendingException(rval);
						goto error;
					}
					JS_ASSERT(rval.isInt32());

					/* Increment the PC by this much. */
					len = rval.toInt32() - int32_t(regs.pc - script->code);
				END_VARLEN_CASE
				}

				BEGIN_CASE(JSOP_EXCEPTION)
					PUSH_COPY(cx->getPendingException());
					cx->clearPendingException();
					CHECK_BRANCH();
				END_CASE(JSOP_EXCEPTION)

				BEGIN_CASE(JSOP_FINALLY)
					CHECK_BRANCH();
				END_CASE(JSOP_FINALLY)

				BEGIN_CASE(JSOP_THROWING)
				{
					JS_ASSERT(!cx->isExceptionPending());
					Value v;
					POP_COPY_TO(v);
					cx->setPendingException(v);
				}
				END_CASE(JSOP_THROWING)

				BEGIN_CASE(JSOP_THROW)
				{
					JS_ASSERT(!cx->isExceptionPending());
					CHECK_BRANCH();
					Value v;
					POP_COPY_TO(v);
					cx->setPendingException(v);
					/* let the code at error try to catch the exception. */
					goto error;
				}

				BEGIN_CASE(JSOP_INSTANCEOF)
				{
					RootedValue &rref = *rootValue0;
					rref = regs.sp[-1];
					if (rref.isPrimitive()) {
						js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS, -1, rref, NullPtr());
						goto error;
					}
					RootedObject &obj = *rootObject0;
					obj = &rref.toObject();
					const Value &lref = regs.sp[-2];
					JSBool cond = JS_FALSE;
					if (!HasInstance(cx, obj, &lref, &cond))
						goto error;
					regs.sp--;
					regs.sp[-1].setBoolean(cond);
				}
				END_CASE(JSOP_INSTANCEOF)

				BEGIN_CASE(JSOP_DEBUGGER)
				{
					printf("Thread(%d) : JSOP_DEBUGGER not Supported", id);
					goto error;
				}
				END_CASE(JSOP_DEBUGGER)

				#if JS_HAS_XML_SUPPORT
				BEGIN_CASE(JSOP_DEFXMLNS)
				{
					JS_ASSERT(!script->strictModeCode);

					if (!js_SetDefaultXMLNamespace(cx, regs.sp[-1]))
						goto error;
					regs.sp--;
				}
				END_CASE(JSOP_DEFXMLNS)

				BEGIN_CASE(JSOP_ANYNAME)
				{
					JS_ASSERT(!script->strictModeCode);

					cx->runtime->gcExactScanningEnabled = false;

					jsid id;
					if (!js_GetAnyName(cx, &id))
						goto error;
					PUSH_COPY(IdToValue(id));
				}
				END_CASE(JSOP_ANYNAME)
				#endif

				BEGIN_CASE(JSOP_QNAMEPART)
					/*
					 * We do not JS_ASSERT(!script->strictModeCode) here because JSOP_QNAMEPART
					 * is used for __proto__ and (in contexts where we favor JSOP_*ELEM instead
					 * of JSOP_*PROP) obj.prop compiled as obj['prop'].
					 */
					PUSH_STRING(script->getAtom(regs.pc));
				END_CASE(JSOP_QNAMEPART)

				#if JS_HAS_XML_SUPPORT
				BEGIN_CASE(JSOP_QNAMECONST)
				{
					JS_ASSERT(!script->strictModeCode);
					Value rval = StringValue(script->getAtom(regs.pc));
					Value lval = regs.sp[-1];
					JSObject *obj = js_ConstructXMLQNameObject(cx, lval, rval);
					if (!obj)
						goto error;
					regs.sp[-1].setObject(*obj);
				}
				END_CASE(JSOP_QNAMECONST)

				BEGIN_CASE(JSOP_QNAME)
				{
					JS_ASSERT(!script->strictModeCode);

					Value rval = regs.sp[-1];
					Value lval = regs.sp[-2];
					JSObject *obj = js_ConstructXMLQNameObject(cx, lval, rval);
					if (!obj)
						goto error;
					regs.sp--;
					regs.sp[-1].setObject(*obj);
				}
				END_CASE(JSOP_QNAME)

				BEGIN_CASE(JSOP_TOATTRNAME)
				{
					JS_ASSERT(!script->strictModeCode);

					Value rval;
					rval = regs.sp[-1];
					if (!js_ToAttributeName(cx, &rval))
						goto error;
					regs.sp[-1] = rval;
				}
				END_CASE(JSOP_TOATTRNAME)

				BEGIN_CASE(JSOP_TOATTRVAL)
				{
					JS_ASSERT(!script->strictModeCode);

					Value rval;
					rval = regs.sp[-1];
					JS_ASSERT(rval.isString());
					JSString *str = js_EscapeAttributeValue(cx, rval.toString(), JS_FALSE);
					if (!str)
						goto error;
					regs.sp[-1].setString(str);
				}
				END_CASE(JSOP_TOATTRVAL)

				BEGIN_CASE(JSOP_ADDATTRNAME)
				BEGIN_CASE(JSOP_ADDATTRVAL)
				{
					JS_ASSERT(!script->strictModeCode);

					Value rval = regs.sp[-1];
					Value lval = regs.sp[-2];
					JSString *str = lval.toString();
					JSString *str2 = rval.toString();
					str = js_AddAttributePart(cx, op == JSOP_ADDATTRNAME, str, str2);
					if (!str)
						goto error;
					regs.sp--;
					regs.sp[-1].setString(str);
				}
				END_CASE(JSOP_ADDATTRNAME)

				BEGIN_CASE(JSOP_BINDXMLNAME)
				{
					JS_ASSERT(!script->strictModeCode);

					Value lval;
					lval = regs.sp[-1];
					RootedObject &obj = *rootObject0;
					jsid id;
					if (!js_FindXMLProperty(cx, lval, &obj, &id))
						goto error;
					regs.sp[-1].setObjectOrNull(obj);
					PUSH_COPY(IdToValue(id));
				}
				END_CASE(JSOP_BINDXMLNAME)

				BEGIN_CASE(JSOP_SETXMLNAME)
				{
					JS_ASSERT(!script->strictModeCode);

					Rooted<JSObject*> obj(cx, &regs.sp[-3].toObject());
					RootedValue &rval = *rootValue0;
					rval = regs.sp[-1];
					RootedId &id = *rootId0;
					FETCH_ELEMENT_ID(obj, -2, id);
					if (!JSObject::setGeneric(cx, obj, obj, id, &rval, script->strictModeCode))
						goto error;
					rval = regs.sp[-1];
					regs.sp -= 2;
					regs.sp[-1] = rval;
				}
				END_CASE(JSOP_SETXMLNAME)

				BEGIN_CASE(JSOP_CALLXMLNAME)
				BEGIN_CASE(JSOP_XMLNAME)
				{
					JS_ASSERT(!script->strictModeCode);

					Value lval = regs.sp[-1];
					RootedObject &obj = *rootObject0;
					RootedId &id = *rootId0;
					if (!js_FindXMLProperty(cx, lval, &obj, id.address()))
						goto error;
					RootedValue &rval = *rootValue0;
					if (!JSObject::getGeneric(cx, obj, obj, id, &rval))
						goto error;
					regs.sp[-1] = rval;
					if (op == JSOP_CALLXMLNAME) {
						Value v;
						if (!ComputeImplicitThis(cx, obj, &v))
							goto error;
						PUSH_COPY(v);
					}
				}
				END_CASE(JSOP_XMLNAME)

				BEGIN_CASE(JSOP_DESCENDANTS)
				BEGIN_CASE(JSOP_DELDESC)
				{
					JS_ASSERT(!script->strictModeCode);

					JSObject *obj;
					FETCH_OBJECT(cx, -2, obj);
					jsval rval = regs.sp[-1];
					if (!js_GetXMLDescendants(cx, obj, rval, &rval))
						goto error;

					if (op == JSOP_DELDESC) {
						regs.sp[-1] = rval;   /* set local root */
						if (!js_DeleteXMLListElements(cx, JSVAL_TO_OBJECT(rval)))
							goto error;
						rval = JSVAL_TRUE;                  /* always succeed */
					}

					regs.sp--;
					regs.sp[-1] = rval;
				}
				END_CASE(JSOP_DESCENDANTS)

				BEGIN_CASE(JSOP_FILTER)
				{
					JS_ASSERT(!script->strictModeCode);

					/*
					 * We push the hole value before jumping to [enditer] so we can detect the
					 * first iteration and direct js_StepXMLListFilter to initialize filter's
					 * state.
					 */
					PUSH_HOLE();
					len = GET_JUMP_OFFSET(regs.pc);
					JS_ASSERT(len > 0);
				}
				END_VARLEN_CASE

				BEGIN_CASE(JSOP_ENDFILTER)
				{
					JS_ASSERT(!script->strictModeCode);

					bool cond = !regs.sp[-1].isMagic();
					if (cond) {
						/* Exit the "with" block left from the previous iteration. */
						regs.fp()->popWith(cx);
					}
					if (!js_StepXMLListFilter(cx, cond))
						goto error;
					if (!regs.sp[-1].isNull()) {
						/*
						 * Decrease sp after EnterWith returns as we use sp[-1] there to root
						 * temporaries.
						 */
						JS_ASSERT(IsXML(regs.sp[-1]));
						if (!EnterWith(cx, -2))
							goto error;
						regs.sp--;
						len = GET_JUMP_OFFSET(regs.pc);
						JS_ASSERT(len < 0);
						BRANCH(len);
					}
					regs.sp--;
				}
				END_CASE(JSOP_ENDFILTER);

				BEGIN_CASE(JSOP_TOXML)
				{
					JS_ASSERT(!script->strictModeCode);

					cx->runtime->gcExactScanningEnabled = false;

					Value rval = regs.sp[-1];
					JSObject *obj = js_ValueToXMLObject(cx, rval);
					if (!obj)
						goto error;
					regs.sp[-1].setObject(*obj);
				}
				END_CASE(JSOP_TOXML)

				BEGIN_CASE(JSOP_TOXMLLIST)
				{
					JS_ASSERT(!script->strictModeCode);

					Value rval = regs.sp[-1];
					JSObject *obj = js_ValueToXMLListObject(cx, rval);
					if (!obj)
						goto error;
					regs.sp[-1].setObject(*obj);
				}
				END_CASE(JSOP_TOXMLLIST)

				BEGIN_CASE(JSOP_XMLTAGEXPR)
				{
					JS_ASSERT(!script->strictModeCode);

					Value rval = regs.sp[-1];
					JSString *str = ToString(cx, rval);
					if (!str)
						goto error;
					regs.sp[-1].setString(str);
				}
				END_CASE(JSOP_XMLTAGEXPR)

				BEGIN_CASE(JSOP_XMLELTEXPR)
				{
					JS_ASSERT(!script->strictModeCode);

					Value rval = regs.sp[-1];
					JSString *str;
					if (IsXML(rval)) {
						str = js_ValueToXMLString(cx, rval);
					} else {
						str = ToString(cx, rval);
						if (str)
							str = js_EscapeElementValue(cx, str);
					}
					if (!str)
						goto error;
					regs.sp[-1].setString(str);
				}
				END_CASE(JSOP_XMLELTEXPR)

				BEGIN_CASE(JSOP_XMLCDATA)
				{
					JS_ASSERT(!script->strictModeCode);

					JSAtom *atom = script->getAtom(regs.pc);
					JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_TEXT, NULL, atom);
					if (!obj)
						goto error;
					PUSH_OBJECT(*obj);
				}
				END_CASE(JSOP_XMLCDATA)

				BEGIN_CASE(JSOP_XMLCOMMENT)
				{
					JS_ASSERT(!script->strictModeCode);

					JSAtom *atom = script->getAtom(regs.pc);
					JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_COMMENT, NULL, atom);
					if (!obj)
						goto error;
					PUSH_OBJECT(*obj);
				}
				END_CASE(JSOP_XMLCOMMENT)

				BEGIN_CASE(JSOP_XMLPI)
				{
					JS_ASSERT(!script->strictModeCode);

					JSAtom *atom = script->getAtom(regs.pc);
					Value rval = regs.sp[-1];
					JSString *str2 = rval.toString();
					JSObject *obj = js_NewXMLSpecialObject(cx, JSXML_CLASS_PROCESSING_INSTRUCTION, atom, str2);
					if (!obj)
						goto error;
					regs.sp[-1].setObject(*obj);
				}
				END_CASE(JSOP_XMLPI)

				BEGIN_CASE(JSOP_GETFUNNS)
				{
					JS_ASSERT(!script->strictModeCode);

					Value rval;
					if (!cx->fp()->global().getFunctionNamespace(cx, &rval))
						goto error;
					PUSH_COPY(rval);
				}
				END_CASE(JSOP_GETFUNNS)
				#endif /* JS_HAS_XML_SUPPORT */

				BEGIN_CASE(JSOP_ENTERBLOCK)
				BEGIN_CASE(JSOP_ENTERLET0)
				BEGIN_CASE(JSOP_ENTERLET1)
				{
					StaticBlockObject &blockObj = script->getObject(regs.pc)->asStaticBlock();

					if (op == JSOP_ENTERBLOCK) {
						JS_ASSERT(regs.stackDepth() == blockObj.stackDepth());
						JS_ASSERT(regs.stackDepth() + blockObj.slotCount() <= script->nslots);
						Value *vp = regs.sp + blockObj.slotCount();
						SetValueRangeToUndefined(regs.sp, vp);
						regs.sp = vp;
					}

					/* Clone block iff there are any closed-over variables. */
					if (!regs.fp()->pushBlock(cx, blockObj))
						goto error;
				}
				END_CASE(JSOP_ENTERBLOCK)

				BEGIN_CASE(JSOP_LEAVEBLOCK)
				BEGIN_CASE(JSOP_LEAVEFORLETIN)
				BEGIN_CASE(JSOP_LEAVEBLOCKEXPR)
				{
					DebugOnly<uint32_t> blockDepth = regs.fp()->blockChain().stackDepth();

					regs.fp()->popBlock(cx);

					if (op == JSOP_LEAVEBLOCK) {
						/* Pop the block's slots. */
						regs.sp -= GET_UINT16(regs.pc);
						JS_ASSERT(regs.stackDepth() == blockDepth);
					} else if (op == JSOP_LEAVEBLOCKEXPR) {
						/* Pop the block's slots maintaining the topmost expr. */
						Value *vp = &regs.sp[-1];
						regs.sp -= GET_UINT16(regs.pc);
						JS_ASSERT(regs.stackDepth() == blockDepth + 1);
						regs.sp[-1] = *vp;
					} else {
						/* Another op will pop; nothing to do here. */
						len = JSOP_LEAVEFORLETIN_LENGTH;
						DO_NEXT_OP(len);
					}
				}
				END_CASE(JSOP_LEAVEBLOCK)

				#if JS_HAS_GENERATORS
				BEGIN_CASE(JSOP_GENERATOR)
				{
					JS_ASSERT(!cx->isExceptionPending());
					regs.fp()->initGeneratorFrame();
					regs.pc += JSOP_GENERATOR_LENGTH;
					JSObject *obj = js_NewGenerator(cx);
					if (!obj)
						goto error;
					regs.fp()->setReturnValue(ObjectValue(*obj));
					regs.fp()->setYielding();
					interpReturnOK = true;
					if (entryFrame != regs.fp())
						goto inline_return;
					goto exit;
				}

				BEGIN_CASE(JSOP_YIELD)
					JS_ASSERT(!cx->isExceptionPending());
					JS_ASSERT(regs.fp()->isNonEvalFunctionFrame());
					if (cx->innermostGenerator()->state == JSGEN_CLOSING) {
						RootedValue &val = *rootValue0;
						val.setObject(regs.fp()->callee());
						js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD, JSDVG_SEARCH_STACK, val, NullPtr());
						goto error;
					}
					regs.fp()->setReturnValue(regs.sp[-1]);
					regs.fp()->setYielding();
					regs.pc += JSOP_YIELD_LENGTH;
					interpReturnOK = true;
					goto exit;

				BEGIN_CASE(JSOP_ARRAYPUSH)
				{
					uint32_t slot = GET_UINT16(regs.pc);
					JS_ASSERT(script->nfixed <= slot);
					JS_ASSERT(slot < script->nslots);
					RootedObject &obj = *rootObject0;
					obj = &regs.fp()->unaliasedLocal(slot).toObject();
					if (!js_NewbornArrayPush(cx, obj, regs.sp[-1]))
						goto error;
					regs.sp--;
				}
				END_CASE(JSOP_ARRAYPUSH)

				default:
				{
					std::cout << "Unimplemented opcode: " << op << std::endl;
					// char numBuf[12];
					// JS_snprintf(numBuf, sizeof numBuf, "%d", op);
					// JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
					//                      JSMSG_BAD_BYTECODE, numBuf);
					// exit(1);
					goto error;
				}
			} /* switch (op) */
		} /* for (;;) */
		
		continue;
		error:
			cout << "ERROR Bailing out\n";
			exit(1);
			
	} //end of big for loop
	
  exit:
    if (cx->compartment->debugMode())
        interpReturnOK = ScriptDebugEpilogue(cx, regs.fp(), interpReturnOK);
    if (!regs.fp()->isYielding())
        regs.fp()->epilogue(cx);
    else
        Probes::exitScript(cx, script, script->function(), regs.fp());
    regs.fp()->setFinishedInInterpreter();

#ifdef JS_METHODJIT
    /*
     * This path is used when it's guaranteed the method can be finished
     * inside the JIT.
     */
  leave_on_safe_point:
#endif

    gc::MaybeVerifyBarriers(cx, true);
    return interpReturnOK;
    //free(cx);
} // end of function ThreadInterpret
