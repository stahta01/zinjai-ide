#ifndef ASSERTS_H
#define ASSERTS_H

/**
* Preprocessor functions to be used instead of c's assert
*
* There are two advantages: 
*  - on debug builds, theses functions will pause the debugger as if a breakpoint where hitted
*  - the name (expect or ensures) implies if we are checking a precondition or a postcondition
*
* There are also two variations (the *_OR ones) that won't fade away in release builds.
* Sometimes, for security/stabillity reason, I want to keep these checks in runtime (but
* avoiding the breakpoint).
**/

#ifdef _ZINJAI_DEBUG
#	define DEBUG_BREAK asm("int3;nop")
#	define EXPECT(cond) if ( !(cond) ) DEBUG_BREAK
#	define ENSURES(cond) if ( !(cond) ) DEBUG_BREAK
#	define EXPECT_OR(cond,else_action) EXPECT(cond); if (!(cond)) else_action
#	define ENSURES_OR(cond,else_action) ENSURES(cond); if (!(cond)) else_action
#else
#	define DEBUG_BREAK
#	define EXPECT_OR(cond,else_action) if ( !(cond) ) else_action
#	define ENSURES_OR(cond,else_action) if ( !(cond) ) else_action
#	define EXPECT(cond) 
#	define ENSURES(cond) 
#endif

#endif
