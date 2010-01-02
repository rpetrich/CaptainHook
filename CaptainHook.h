// Possible defines:
//	CHDebug           if defined, CHDebugLog is equivalent to CHLog; else, emits no code
//	CHUseSubstrate    if defined, uses MSMessageHookEx to hook methods, otherwise uses internal hooking routines. Warning! super call closures are only available on ARM platforms for recent releases of MobileSubstrate
//	CHEnableProfiling if defined, enables calls to CHProfileScope()
//  CHAppName         should be set to the name of the application (if not, defaults to "CaptainHook"); used for logging and profiling

#import <objc/runtime.h>
#import <objc/message.h>
#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

#ifndef CHAppName
#define CHAppName "CaptainHook"
#endif

// Some Debugging/Logging Commands

#define CHStringify_(x) #x
#define CHStringify(x) CHStringify_(x)
#define CHConcat_(a, b) a ## b
#define CHConcat(a, b) CHConcat_(a, b)

#define CHNothing() do { } while(0)

#define CHLocationInSource [NSString stringWithFormat:@CHStringify(__LINE__) " in %s", __FUNCTION__]

#define CHLog(args...)			NSLog(@CHAppName ": %@", [NSString stringWithFormat:args])
#define CHLogSource(args...)	NSLog(@CHAppName " @ " CHStringify(__LINE__) " in %s: %@", __FUNCTION__, [NSString stringWithFormat:args])

#ifdef CHDebug
	#define CHDebugLog(args...)			CHLog(args)
	#define CHDebugLogSource(args...)	CHLogSource(args)
#else
	#define CHDebugLog(args...)			CHNothing()
	#define CHDebugLogSource(args...)	CHNothing()
#endif

// Constructor
#define CHConstructor static __attribute__((constructor)) void CHConcat(CHConstructor, __LINE__)()
#define CHInline inline __attribute__((always_inline))

// Retrieveing classes/testing against objects

struct CHClassDeclaration_ {
	id class_;
	id metaClass_;
	id superClass_;
};
typedef struct CHClassDeclaration_ CHClassDeclaration_;

#define CHDeclareClass(name) \
	@class name; \
	static CHClassDeclaration_ name ## $;
	
#define CHLoadLateClass(name) do { \
	CHClass(name) = objc_getClass(#name); \
	CHMetaClass(name) = object_getClass(CHClass(name)); \
	CHSuperClass(name) = class_getSuperclass(CHClass(name)); \
} while(0)
#define CHLoadClass(name) do { \
	CHClass(name) = [name class]; \
	CHMetaClass(name) = object_getClass(CHClass(name)); \
	CHSuperClass(name) = class_getSuperclass(CHClass(name)); \
} while(0)

#define CHClass(name) name ## $.class_
#define CHMetaClass(name) name ## $.metaClass_
#define CHSuperClass(name) name ## $.superClass_
#define CHAlloc(name) ((name *)[CHClass(name) alloc])
#define CHSharedInstance(name) ((name *)[CHClass(name) sharedInstance])
#define CHIsClass(obj, name) [obj isKindOfClass:CHClass(name)]
#define CHRespondsTo(obj, sel) [obj respondsToSelector:@selector(sel)]

// To Load classes for hooking

#ifdef CHUseSubstrate
#import <substrate.h>
#define CHHook_(className, impName, classVar, sel, sigdef) ({ \
	SEL selector = sel; \
	MSHookMessageEx(classVar, selector, (IMP)&$ ## className ## _ ## impName, (IMP *)&_ ## className ## _ ## impName); \
	if (!_ ## className ## _ ## impName) { \
		sigdef; \
		class_addMethod(classVar, selector, (IMP)&$ ## className ## _ ## impName, sig); \
	} \
})
#else
#define CHHook_(className, impName, classVal, sel, sigdef) ({ \
	Class classVar = classVal; \
	if (classVar) { \
		SEL selector = sel; \
		Method method = class_getInstanceMethod(classVar, selector); \
		if (method) { \
			_ ## className ## _ ## impName = &$$ ## className ## _ ## impName; \
			if (!class_addMethod(classVar, selector, (IMP)&$ ## className ## _ ## impName, method_getTypeEncoding(method))) { \
				_ ## className ## _ ## impName = (__typeof__(_ ## className ## _ ## impName))method_getImplementation(method); \
				method_setImplementation(method, (IMP)&$ ## className ## _ ## impName); \
			} \
		} else { \
			sigdef; \
			NSLog(@"CaptainHook " #className " " #impName " %s", sig); \
			class_addMethod(classVar, selector, (IMP)&$ ## className ## _ ## impName, sig); \
		} \
	} \
})
#endif

// Optimizer has fun with these; should be as quick as we can get without requiring consumers to put a constant type encoding in: "@@:"
#define CHDeclareSig0_(return_type) \
	const char *return_ = @encode(return_type); \
	size_t return_len = strlen(return_); \
	char sig[return_len+2]; \
	memcpy(sig, return_, return_len); \
	sig[return_len] = _C_ID; \
	sig[return_len+1] = _C_SEL; \
	sig[return_len+2] = '\0';
#define CHDeclareSig1_(return_type, type1) \
	const char *return_ = @encode(return_type); \
	size_t return_len = strlen(return_); \
	const char *type1_ = @encode(type1); \
	size_t type1_len = strlen(type1_); \
	char sig[return_len+2+type1_len]; \
	memcpy(sig, return_, return_len); \
	sig[return_len] = _C_ID; \
	sig[return_len+1] = _C_SEL; \
	memcpy(&sig[return_len+2], type1_, type1_len); \
	sig[return_len+type1_len+2] = '\0';
#define CHDeclareSig2_(return_type, type1, type2) \
	const char *return_ = @encode(return_type); \
	size_t return_len = strlen(return_); \
	const char *type1_ = @encode(type1); \
	size_t type1_len = strlen(type1_); \
	const char *type2_ = @encode(type2); \
	size_t type2_len = strlen(type2_); \
	char sig[return_len+2+type1_len+type2_len]; \
	memcpy(sig, return_, return_len); \
	sig[return_len] = _C_ID; \
	sig[return_len+1] = _C_SEL; \
	memcpy(&sig[return_len+2], type1_, type1_len); \
	memcpy(&sig[return_len+2+type1_len], type2_, type2_len); \
	sig[return_len+type1_len+type2_len+2] = '\0';
#define CHDeclareSig3_(return_type, type1, type2, type3) \
	const char *return_ = @encode(return_type); \
	size_t return_len = strlen(return_); \
	const char *type1_ = @encode(type1); \
	size_t type1_len = strlen(type1_); \
	const char *type2_ = @encode(type2); \
	size_t type2_len = strlen(type2_); \
	const char *type3_ = @encode(type3); \
	size_t type3_len = strlen(type3_); \
	char sig[return_len+2+type1_len+type2_len+type3_len]; \
	memcpy(sig, return_, return_len); \
	sig[return_len] = _C_ID; \
	sig[return_len+1] = _C_SEL; \
	memcpy(&sig[return_len+2], type1_, type1_len); \
	memcpy(&sig[return_len+2+type1_len], type2_, type2_len); \
	memcpy(&sig[return_len+2+type1_len+type2_len], type3_, type3_len); \
	sig[return_len+type1_len+type2_len+type3_len+2] = '\0';
#define CHDeclareSig4_(return_type, type1, type2, type3, type4) \
	const char *return_ = @encode(return_type); \
	size_t return_len = strlen(return_); \
	const char *type1_ = @encode(type1); \
	size_t type1_len = strlen(type1_); \
	const char *type2_ = @encode(type2); \
	size_t type2_len = strlen(type2_); \
	const char *type3_ = @encode(type3); \
	size_t type3_len = strlen(type3_); \
	const char *type4_ = @encode(type4); \
	size_t type4_len = strlen(type4_); \
	char sig[return_len+2+type1_len+type2_len+type3_len+type4_len]; \
	memcpy(sig, return_, return_len); \
	sig[return_len] = _C_ID; \
	sig[return_len+1] = _C_SEL; \
	memcpy(&sig[return_len+2], type1_, type1_len); \
	memcpy(&sig[return_len+2+type1_len], type2_, type2_len); \
	memcpy(&sig[return_len+2+type1_len+type2_len], type3_, type3_len); \
	memcpy(&sig[return_len+2+type1_len+type2_len+type3_len], type4_, type4_len); \
	sig[return_len+type1_len+type2_len+type3_len+type4_len+2] = '\0';
#define CHDeclareSig5_(return_type, type1, type2, type3, type4, type5) \
	const char *return_ = @encode(return_type); \
	size_t return_len = strlen(return_); \
	const char *type1_ = @encode(type1); \
	size_t type1_len = strlen(type1_); \
	const char *type2_ = @encode(type2); \
	size_t type2_len = strlen(type2_); \
	const char *type3_ = @encode(type3); \
	size_t type3_len = strlen(type3_); \
	const char *type4_ = @encode(type4); \
	size_t type4_len = strlen(type4_); \
	const char *type5_ = @encode(type5); \
	size_t type5_len = strlen(type5_); \
	char sig[return_len+2+type1_len+type2_len+type3_len+type4_len+type5_len]; \
	memcpy(sig, return_, return_len); \
	sig[return_len] = _C_ID; \
	sig[return_len+1] = _C_SEL; \
	memcpy(&sig[return_len+2], type1_, type1_len); \
	memcpy(&sig[return_len+2+type1_len], type2_, type2_len); \
	memcpy(&sig[return_len+2+type1_len+type2_len], type3_, type3_len); \
	memcpy(&sig[return_len+2+type1_len+type2_len+type3_len], type4_, type4_len); \
	memcpy(&sig[return_len+2+type1_len+type2_len+type3_len+type4_len], type5_, type5_len); \
	sig[return_len+type1_len+type2_len+type3_len+type4_len+type5_len+2] = '\0';
	
#define CHDeclareDummySig_() const char *sig = "@@:";

#define CHHook(class, imp) CHHook_(class, imp, CHClass(class), CHSelFromImpName(imp), CHDeclareDummySig_())
#define CHHook0(class, name) CHHook_(class, name, CHClass(class), @selector(name), CHDeclareDummySig_())
#define CHHook1(class, name1) CHHook_(class, name1 ## $, CHClass(class), @selector(name1:), CHDeclareDummySig_())
#define CHHook2(class, name1, name2) CHHook_(class, name1 ## $ ## name2 ## $, CHClass(class), @selector(name1:name2:), CHDeclareDummySig_())
#define CHHook3(class, name1, name2, name3) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $, CHClass(class), @selector(name1:name2:name3:), CHDeclareDummySig_())
#define CHHook4(class, name1, name2, name3, name4) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, CHClass(class), @selector(name1:name2:name3:name4:), CHDeclareDummySig_())
#define CHHook5(class, name1, name2, name3, name4, name5) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ ## name5 ## $, CHClass(class), @selector(name1:name2:name3:name4:name5:), CHDeclareDummySig_())

#define CHAddHook0(return_type, class, name) CHHook_(class, name, CHClass(class), @selector(name), CHDeclareSig0_(return_type))
#define CHAddHook1(return_type, class, name1, type1) CHHook_(class, name1 ## $, CHClass(class), @selector(name1:), CHDeclareSig1_(return_type, type1))
#define CHAddHook2(return_type, class, name1, type1, name2, type2) CHHook_(class, name1 ## $ ## name2 ## $, CHClass(class), @selector(name1:name2:), CHDeclareSig2_(return_type, type1, type2))
#define CHAddHook3(return_type, class, name1, type1, name2, type2, name3, type3) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $, CHClass(class), @selector(name1:name2:name3:), CHDeclareSig3_(return_type, type1, type2, type3))
#define CHAddHook4(return_type, class, name1, type1, name2, type2, name3, type3, name4, type4) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, CHClass(class), @selector(name1:name2:name3:name4:), CHDeclareSig4_(return_type, type1, type2, type3, type4))
#define CHAddHook5(return_type, class, name1, type1, name2, type2, name3, type3, name4, type4, name5, type5) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ ## name5 ## $, CHClass(class), @selector(name1:name2:name3:name4:name5:), CHDeclareSig5_(return_type, type1, type2, type3, type4, type5))

#define CHClassHook(class, imp) CHHook_(class, imp, CHMetaClass(class), CHSelFromImpName(imp), CHDeclareDummySig_())
#define CHClassHook0(class, name) CHHook_(class, name, CHMetaClass(class), @selector(name), CHDeclareDummySig_())
#define CHClassHook1(class, name1) CHHook_(class, name1 ## $, CHMetaClass(class), @selector(name1:), CHDeclareDummySig_())
#define CHClassHook2(class, name1, name2) CHHook_(class, name1 ## $ ## name2 ## $, CHMetaClass(class), @selector(name1:name2:), CHDeclareDummySig_())
#define CHClassHook3(class, name1, name2, name3) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $, CHMetaClass(class), @selector(name1:name2:name3:), CHDeclareDummySig_())
#define CHClassHook4(class, name1, name2, name3, name4) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, CHMetaClass(class), @selector(name1:name2:name3:name4:), CHDeclareDummySig_())
#define CHClassHook5(class, name1, name2, name3, name4, name5) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ ## name5 ## $, CHMetaClass(class), @selector(name1:name2:name3:name4:name5:), CHDeclareDummySig_())

#define CHAddClassHook0(return_type, class, name) CHHook_(class, name, CHMetaClass(class), @selector(name), CHDeclareSig0_(return_type))
#define CHAddClassHook1(return_type, class, name1, type1) CHHook_(class, name1 ## $, CHMetaClass(class), @selector(name1:), CHDeclareSig1_(return_type, type1))
#define CHAddClassHook2(return_type, class, name1, type1, name2, type2) CHHook_(class, name1 ## $ ## name2 ## $, CHMetaClass(class), @selector(name1:name2:), CHDeclareSig2_(return_type, type1, type2))
#define CHAddClassHook3(return_type, class, name1, type1, name2, type2, name3, type3) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $, CHMetaClass(class), @selector(name1:name2:name3:), CHDeclareSig3_(return_type, type1, type2, type3))
#define CHAddClassHook4(return_type, class, name1, type1, name2, type2, name3, type3, name4, type4) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, CHMetaClass(class), @selector(name1:name2:name3:name4:), CHDeclareSig4_(return_type, type1, type2, type3, type4))
#define CHAddClassHook5(return_type, class, name1, type1, name2, type2, name3, type3, name4, type4, name5, type5) CHHook_(class, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ ## name5 ## $, CHMetaClass(class), @selector(name1:name2:name3:name4:name5:), CHDeclareSig5_(return_type, type1, type2, type3, type4, type5))

// For Replacement Functions
#ifdef CHUseSubstrate
#define CHMethod_(return_type, class_type, class_name, name, supercall, args...) \
	@class class_name; \
	static return_type $ ## class_name ## _ ## name(class_type self, SEL _cmd, ##args)
#else
#define CHMethod_(return_type, class_type, class_name, name, supercall, args...) \
	@class class_name; \
	static return_type (*_ ## class_name ## _ ## name)(class_name *self, SEL _cmd, ##args); \
	static return_type $$ ## class_name ## _ ## name(class_name *self, SEL _cmd, ##args) { \
		typedef return_type (*supType)(id, SEL, ## args); \
		supType supFn = (supType)class_getMethodImplementation(CHSuperClass(class_name), _cmd); \
		return supFn supercall; \
	} \
	static return_type $ ## class_name ## _ ## name(class_type self, SEL _cmd, ##args)
#endif
#define CHMethod0(return_type, class_type, name) \
	CHMethod_(return_type, class_type *, class_type, name, (self, _cmd))
#define CHMethod1(return_type, class_type, name1, type1, arg1) \
	CHMethod_(return_type, class_type *, class_type, name1 ## $, (self, _cmd, arg1), type1 arg1)
#define CHMethod2(return_type, class_type, name1, type1, arg1, name2, type2, arg2) \
	CHMethod_(return_type, class_type *, class_type, name1 ## $ ## name2 ## $, (self, _cmd, arg1, arg2), type1 arg1, type2 arg2)
#define CHMethod3(return_type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3) \
	CHMethod_(return_type, class_type *, class_type, name1 ## $ ## name2 ## $ ## name3 ## $, (self, _cmd, arg1, arg2, arg3), type1 arg1, type2 arg2, type3 arg3)
#define CHMethod4(return_type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3, name4, type4, arg4) \
	CHMethod_(return_type, class_type *, class_type, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, (self, _cmd, arg1, arg2, arg3, arg4), type1 arg1, type2 arg2, type3 arg3, type4 arg4)
#define CHMethod5(return_type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3, name4, type4, arg4, name5, type5, arg5) \
	CHMethod_(return_type, class_type *, class_type, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ ## arg5 ## $, (self, _cmd, arg1, arg2, arg3, arg4, arg5), type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5)
#define CHClassMethod0(return_type, class_type, name) \
	CHMethod_(return_type, id, class_type, name, (self, _cmd))
#define CHClassMethod1(return_type, class_type, name1, type1, arg1) \
	CHMethod_(return_type, id, class_type, name1 ## $, (self, _cmd, arg1), type1 arg1)
#define CHClassMethod2(return_type, class_type, name1, type1, arg1, name2, type2, arg2) \
	CHMethod_(return_type, id, class_type, name1 ## $ ## name2 ## $, (self, _cmd, arg1, arg2), type1 arg1, type2 arg2)
#define CHClassMethod3(return_type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3) \
	CHMethod_(return_type, id, class_type, name1 ## $ ## name2 ## $ ## name3 ## $, (self, _cmd, arg1, arg2, arg3), type1 arg1, type2 arg2, type3 arg3)
#define CHClassMethod4(return_type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3, name4, type4, arg4) \
	CHMethod_(return_type, id, class_type, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, (self, _cmd, arg1, arg2, arg3, arg4), type1 arg1, type2 arg2, type3 arg3, type4 arg4)
#define CHClassMethod5(return_type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3, name4, type4, arg4, name5, type5, arg5) \
	CHMethod_(return_type, id, class_type, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ ## name5 ## $, (self, _cmd, arg1, arg2, arg3, arg4, arg5), type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5)
		
#define CHSuper_(class_type, _cmd, name, args...) \
	_ ## class_type ## _ ## name(self, _cmd, ##args)
#define CHSuper0(class_type, name) \
	CHSuper_(class_type, @selector(name), name)
#define CHSuper1(class_type, name1, val1) \
	CHSuper_(class_type, @selector(name1:), name1 ## $, val1)
#define CHSuper2(class_type, name1, val1, name2, val2) \
	CHSuper_(class_type, @selector(name1:name2:), name1 ## $ ## name2 ## $, val1, val2)
#define CHSuper3(class_type, name1, val1, name2, val2, name3, val3) \
	CHSuper_(class_type, @selector(name1:name2:name3:), name1 ## $ ## name2 ## $ ## name3 ## $, val1, val2, val3)
#define CHSuper4(class_type, name1, val1, name2, val2, name3, val3, name4, val4) \
	CHSuper_(class_type, @selector(name1:name2:name3:name4:), name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, val1, val2, val3, val4)
#define CHSuper5(class_type, name1, val1, name2, val2, name3, val3, name4, val4, name5, val5) \
	CHSuper_(class_type, @selector(name1:name2:name3:name4:name5:), name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ ## name5 ## $, val1, val2, val3, val4, val5)

// Declarative-style

#define CHDeclareMethod_(type, class_type, class_name, name, supercall, sel, args...) \
	@class class_name; \
	static type (*_ ## class_name ## _ ## name)(class_name *self, SEL _cmd, ##args); \
	static type $$ ## class_name ## _ ## name(class_name *self, SEL _cmd, ##args); \
	static type $ ## class_name ## _ ## name(class_type self, SEL _cmd, ##args); \
	CHConstructor { \
		CHLoadLateClass(class_name); \
		CHHook_(class_name, name, CHClass(class_name), @selector(sel)); \
	} \
	CHMethod_(type, class_type, class_name, name, supercall, ##args)
#define CHDeclareMethod0(type, class_type, name) \
	CHDeclareMethod_(type, class_type *, class_type, name, (self, _cmd), name)
#define CHDeclareMethod1(type, class_type, name1, type1, arg1) \
	CHDeclareMethod_(type, class_type *, class_type, name1 ## $, (self, _cmd, arg1), name1:, type1 arg1)
#define CHDeclareMethod2(type, class_type, name1, type1, arg1, name2, type2, arg2) \
	CHDeclareMethod_(type, class_type *, class_type, name1 ## $ ## name2 ## $, (self, _cmd, arg1, arg2), name1:name2:, type1 arg1, type2 arg2)
#define CHDeclareMethod3(type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3) \
	CHDeclareMethod_(type, class_type *, class_type, name1 ## $ ## name2 ## $ ## name3 ## $, (self, _cmd, arg1, arg2, arg3), name1:name2:name3:, type1 arg1, type2 arg2, type3 arg3)
#define CHDeclareMethod4(type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3, name4, type4, arg4) \
	CHDeclareMethod_(type, class_type *, class_type, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, (self, _cmd, arg1, arg2, arg3, arg4), name1:name2:name3:name4:, type1 arg1, type2 arg2, type3 arg3, type4 arg4)
#define CHDeclareMethod5(type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3, name4, type4, arg4, name5, type5, arg5) \
	CHDeclareMethod_(type, class_type *, class_type, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ name5 ## $, (self, _cmd, arg1, arg2, arg3, arg4, arg5), name1:name2:name3:name4:name5:, type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5)

#define CHDeclareClassMethod_(type, class_type, class_name, name, supercall, sel, args...) \
	@class class_name; \
	static type (*_ ## class_name ## _ ## name)(class_name *self, SEL _cmd, ##args); \
	static type $$ ## class_name ## _ ## name(class_name *self, SEL _cmd, ##args); \
	static type $ ## class_name ## _ ## name(class_type self, SEL _cmd, ##args); \
	CHConstructor { \
		CHLoadLateClass(class_name); \
		CHHook_(class_name, name, object_getClass(CHClass(class_name)), @selector(sel)); \
	} \
	CHMethod_(type, class_type, class_name, name, supercall, ##args)
#define CHDeclareClassMethod0(type, class_type, name) \
	CHDeclareClassMethod_(type, id, class_type, name, (self, _cmd), name)
#define CHDeclareClassMethod1(type, class_type, name1, type1, arg1) \
	CHDeclareClassMethod_(type, id, class_type, name1 ## $, (self, _cmd, arg1), name1:, type1 arg1)
#define CHDeclareClassMethod2(type, class_type, name1, type1, arg1, name2, type2, arg2) \
	CHDeclareClassMethod_(type, id, class_type, name1 ## $ ## name2 ## $, (self, _cmd, arg1, arg2), name1:name2:, type1 arg1, type2 arg2)
#define CHDeclareClassMethod3(type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3) \
	CHDeclareClassMethod_(type, id, class_type, name1 ## $ ## name2 ## $ ## name3 ## $, (self, _cmd, arg1, arg2, arg3), name1:name2:name3:, type1 arg1, type2 arg2, type3 arg3)
#define CHDeclareClassMethod4(type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3, name4, type4, arg4) \
	CHDeclareClassMethod_(type, id, class_type, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $, (self, _cmd, arg1, arg2, arg3, arg4), name1:name2:name3:name4:, type1 arg1, type2 arg2, type3 arg3, type4 arg4)
#define CHDeclareClassMethod5(type, class_type, name1, type1, arg1, name2, type2, arg2, name3, type3, arg3, name4, type4, arg4, name5, type5, arg5) \
	CHDeclareClassMethod_(type, id, class_type, name1 ## $ ## name2 ## $ ## name3 ## $ ## name4 ## $ name5 ## $, (self, _cmd, arg1, arg2, arg3, arg4, arg5), name1:name2:name3:name4:name5:, type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5)
	
#define CHRegisterClass(name, superName) for (int _tmp = ({ CHClass(name) = objc_allocateClassPair(CHClass(superName), #name, 0); CHMetaClass(name) = object_getClass(CHClass(name)); CHSuperClass(name) = class_getSuperclass(CHClass(name)); 1; }); _tmp; _tmp = ({ objc_registerClassPair(CHClass(name)), 0; }))

// Add Ivar to a new class at runtime
#define CHAddIvar(targetClass, name, type) \
	class_addIvar(targetClass, #name, sizeof(type), log2(sizeof(type)), @encode(type))

// Retrieve reference to an Ivar value (can read and assign)
__attribute__((unused)) CHInline
static void *CHIvar_(id object, const char *name)
{
	Ivar ivar = class_getInstanceVariable(object_getClass(object), name);
	if (ivar)
		return (void *)&((char *)object)[ivar_getOffset(ivar)];
	return NULL;
}
#define CHIvarRef(object, name, type) \
	((type *)CHIvar_(object, #name))
#define CHIvar(object, name, type) \
	(*CHIvarRef(object, name, type))
	// Warning: Dereferences NULL if object is nil or name isn't found. To avoid this save CHIvarRef(...) and test if != NULL

// Scope Autorelease
__attribute__((unused)) CHInline
static void CHScopeReleased(id sro)
{
    [sro release];
}
#define CHScopeReleased \
	__attribute__((cleanup(CHScopeReleased)))

#ifdef __cplusplus
	extern "C" void *NSPushAutoreleasePool(NSUInteger capacity);
	extern "C" void NSPopAutoreleasePool(void *token);
#else
	void *NSPushAutoreleasePool(NSUInteger capacity);
	void NSPopAutoreleasePool(void *token);
#endif
#define CHAutoreleasePoolForScope() \
	void *CHAutoreleasePoolForScope __attribute__((unused)) __attribute__((cleanup(NSPopAutoreleasePool))) = NSPushAutoreleasePool(0)

#define CHBuildAssert(condition) \
	((void)sizeof(char[1 - 2*!!(condition)]))

// Profiling

#ifdef CHEnableProfiling
	#import <mach/mach_time.h>
	struct CHProfileData
	{
		NSString *message;
		uint64_t startTime;
	};
	__attribute__((unused)) CHInline
	static void CHInline CHProfileCalculateDurationAndLog_(struct CHProfileData *profileData)
	{
		uint64_t duration = mach_absolute_time() - profileData->startTime;
		mach_timebase_info_data_t info;
		mach_timebase_info(&info);
		duration = (duration * info.numer) / info.denom;
		CHLog(@"Profile time: %lldns; %@", duration, profileData->message);
	}
	#define CHProfileScopeWithString(string) \
		struct CHProfileData _profileData __attribute__((cleanup(CHProfileCalculateDurationAndLog_))) = ({ struct CHProfileData _tmp; _tmp.message = (string); _tmp.startTime = mach_absolute_time(); _tmp; })
#else
	#define CHProfileScopeWithString(string) \
		CHNothing()
#endif
#define CHProfileScopeWithFormat(args...) \
	CHProfileScopeWithString(([NSString stringWithFormat:args]))
#define CHProfileScope() \
	CHProfileScopeWithFormat(@CHStringify(__LINE__) " in %s", __FUNCTION__)
