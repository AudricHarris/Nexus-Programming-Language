; ModuleID = 'nexus'
source_filename = "nexus"
target triple = "x86_64-unknown-linux-gnu"

@.fmt = private unnamed_addr constant [30 x i8] c"The number %lld is Even.\0A\1B[0m\00", align 1
@.fmt.1 = private unnamed_addr constant [29 x i8] c"The number %lld is Odd.\0A\1B[0m\00", align 1
@.fmt.2 = private unnamed_addr constant [26 x i8] c"Last Message : {tab}\0A\1B[0m\00", align 1
@.fmt.3 = private unnamed_addr constant [30 x i8] c"%lld is bitween 5 and 15\0A\1B[0m\00", align 1
@.fmt.4 = private unnamed_addr constant [15 x i8] c"Non valid : 1\0A\00", align 1
@.fmt.5 = private unnamed_addr constant [30 x i8] c"15 is bitween 5 and %lld\0A\1B[0m\00", align 1
@.fmt.6 = private unnamed_addr constant [15 x i8] c"Non valid : 2\0A\00", align 1

declare i32 @printf(ptr, ...)

declare i32 @strcmp(ptr, ptr)

declare i32 @scanf(ptr, ptr)

define i32 @"RandomInt$1"(i32 %0) #0 {
entry:
  %end = alloca i32, align 4
  store i32 %0, ptr %end, align 4
  %rand.val = call i32 @rand()
  %rand.f = sitofp i32 %rand.val to double
  %random.f = fdiv double %rand.f, 0x41DFFFFFFFC00000
  %end.load = load i32, ptr %end, align 4
  %itof = sitofp i32 %end.load to double
  %fmul = fmul double %random.f, %itof
  %cast.fptosi = fptosi double %fmul to i32
  ret i32 %cast.fptosi
}

define i32 @"RandomInt$2"(i32 %0, i32 %1) #0 {
entry:
  %gap = alloca i32, align 4
  %tmp = alloca i32, align 4
  %end = alloca i32, align 4
  %start = alloca i32, align 4
  store i32 %0, ptr %start, align 4
  store i32 %1, ptr %end, align 4
  %start.load = load i32, ptr %start, align 4
  %end.load = load i32, ptr %end, align 4
  %igt = icmp sgt i32 %start.load, %end.load
  br i1 %igt, label %then, label %else

then:                                             ; preds = %entry
  %start.load1 = load i32, ptr %start, align 4
  store i32 %start.load1, ptr %tmp, align 4
  %end.load2 = load i32, ptr %end, align 4
  store i32 %end.load2, ptr %start, align 4
  %tmp.load = load i32, ptr %tmp, align 4
  store i32 %tmp.load, ptr %end, align 4
  br label %merge

else:                                             ; preds = %entry
  br label %merge

merge:                                            ; preds = %else, %then
  %end.load3 = load i32, ptr %end, align 4
  %start.load4 = load i32, ptr %start, align 4
  %sub = sub i32 %end.load3, %start.load4
  store i32 %sub, ptr %gap, align 4
  %rand.val = call i32 @rand()
  %rand.f = sitofp i32 %rand.val to double
  %random.f = fdiv double %rand.f, 0x41DFFFFFFFC00000
  %gap.load = load i32, ptr %gap, align 4
  %itof = sitofp i32 %gap.load to double
  %fmul = fmul double %random.f, %itof
  %start.load5 = load i32, ptr %start, align 4
  %itof6 = sitofp i32 %start.load5 to double
  %fadd = fadd double %fmul, %itof6
  %cast.fptosi = fptosi double %fadd to i32
  ret i32 %cast.fptosi
}

define void @"Randomize$1"(ptr %0) #0 {
entry:
  %t.refptr = alloca ptr, align 8
  store ptr %0, ptr %t.refptr, align 8
  %call = call i32 @"RandomInt$2"(i32 0, i32 100)
  %t.ref = load ptr, ptr %t.refptr, align 8
  store i32 %call, ptr %t.ref, align 4
  ret void
}

define i1 @main() #0 {
entry:
  %y = alloca i32, align 4
  %x = alloca i32, align 4
  %t = alloca i32, align 4
  %time.val = call i64 @time(ptr null)
  %seed = trunc i64 %time.val to i32
  call void @srand(i32 %seed)
  br label %while.cond

while.cond:                                       ; preds = %merge6, %then, %entry
  br i1 true, label %while.body, label %while.exit

while.body:                                       ; preds = %while.cond
  call void @"Randomize$1"(ptr %t)
  %t.load = load i32, ptr %t, align 4
  %srem = srem i32 %t.load, 2
  %ieq = icmp eq i32 %srem, 0
  br i1 %ieq, label %then, label %else

then:                                             ; preds = %while.body
  %t.load1 = load i32, ptr %t, align 4
  %0 = sext i32 %t.load1 to i64
  %printf.ret = call i32 (ptr, ...) @printf(ptr @.fmt, i64 %0)
  br label %while.cond

else:                                             ; preds = %while.body
  br label %merge

merge:                                            ; preds = %else
  %t.load2 = load i32, ptr %t, align 4
  %ieq3 = icmp eq i32 %t.load2, 5
  br i1 %ieq3, label %then4, label %else5

then4:                                            ; preds = %merge
  br label %while.exit

else5:                                            ; preds = %merge
  br label %merge6

merge6:                                           ; preds = %else5
  %t.load7 = load i32, ptr %t, align 4
  %1 = sext i32 %t.load7 to i64
  %printf.ret8 = call i32 (ptr, ...) @printf(ptr @.fmt.1, i64 %1)
  br label %while.cond

while.exit:                                       ; preds = %then4, %while.cond
  %printf.ret9 = call i32 (ptr, ...) @printf(ptr @.fmt.2)
  %call = call i32 @"RandomInt$2"(i32 10, i32 20)
  store i32 %call, ptr %x, align 4
  store i32 15, ptr %y, align 4
  %x.load = load i32, ptr %x, align 4
  %ilt = icmp slt i32 5, %x.load
  %x.load10 = load i32, ptr %x, align 4
  %y.load = load i32, ptr %y, align 4
  %ilt11 = icmp slt i32 %x.load10, %y.load
  %l.bool = icmp ne i1 %ilt, false
  %r.bool = icmp ne i1 %ilt11, false
  %land = and i1 %l.bool, %r.bool
  br i1 %land, label %then12, label %else15

then12:                                           ; preds = %while.exit
  %x.load13 = load i32, ptr %x, align 4
  %2 = sext i32 %x.load13 to i64
  %printf.ret14 = call i32 (ptr, ...) @printf(ptr @.fmt.3, i64 %2)
  br label %merge17

else15:                                           ; preds = %while.exit
  %printf.ret16 = call i32 (ptr, ...) @printf(ptr @.fmt.4)
  br label %merge17

merge17:                                          ; preds = %else15, %then12
  %y.load18 = load i32, ptr %y, align 4
  %ilt19 = icmp slt i32 5, %y.load18
  %x.load20 = load i32, ptr %x, align 4
  %ilt21 = icmp slt i32 %y.load18, %x.load20
  %chain.and = and i1 %ilt19, %ilt21
  br i1 %chain.and, label %then22, label %else25

then22:                                           ; preds = %merge17
  %x.load23 = load i32, ptr %x, align 4
  %3 = sext i32 %x.load23 to i64
  %printf.ret24 = call i32 (ptr, ...) @printf(ptr @.fmt.5, i64 %3)
  br label %merge27

else25:                                           ; preds = %merge17
  %printf.ret26 = call i32 (ptr, ...) @printf(ptr @.fmt.6)
  br label %merge27

merge27:                                          ; preds = %else25, %then22
  ret i1 false
}

declare i32 @rand()

declare i64 @time(ptr)

declare void @srand(i32)

attributes #0 = { "stackrealignment" }
