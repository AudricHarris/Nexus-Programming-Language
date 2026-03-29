; ModuleID = 'nexus'
source_filename = "nexus"
target triple = "x86_64-unknown-linux-gnu"

%array.array.i8 = type { i64, ptr }
%array.i8 = type { i64, ptr }
%array.array.bool = type { i64, ptr }
%array.bool = type { i64, ptr }
%string = type { ptr, i64, i64 }
%array.array.array.i8 = type { i64, ptr }
%array.array.array.bool = type { i64, ptr }

@strl = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@strl.1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@strl.2 = private unnamed_addr constant [4 x i8] c"\E2\94\80\00", align 1
@.fmt = private unnamed_addr constant [14 x i8] c"\E2\94\8C%s\E2\94\90\0A\1B[0m\00", align 1
@.fmt.3 = private unnamed_addr constant [8 x i8] c"\E2\94\82\1B[0m\00", align 1
@.fmt.4 = private unnamed_addr constant [22 x i8] c"\1B[38;2;0;0;255m\C2\A4\1B[0m\00", align 1
@.fmt.5 = private unnamed_addr constant [23 x i8] c"\1B[38;2;255;255;0mF\1B[0m\00", align 1
@.fmt.6 = private unnamed_addr constant [21 x i8] c"\1B[38;2;0;119;0m#\1B[0m\00", align 1
@.fmt.7 = private unnamed_addr constant [21 x i8] c"\1B[38;2;255;0;0m*\1B[0m\00", align 1
@.fmt.8 = private unnamed_addr constant [7 x i8] c"%c\1B[0m\00", align 1
@.fmt.9 = private unnamed_addr constant [20 x i8] c"\1B[38;2;0;68;0m#\1B[0m\00", align 1
@.fmt.10 = private unnamed_addr constant [9 x i8] c"\E2\94\82\0A\1B[0m\00", align 1
@.fmt.11 = private unnamed_addr constant [14 x i8] c"\E2\94\94%s\E2\94\98\0A\1B[0m\00", align 1
@.fmt.12 = private unnamed_addr constant [46 x i8] c"WASD = move | O = open | F = flag | Q = quit\0A\00", align 1
@.fmt.13 = private unnamed_addr constant [24 x i8] c"%lld , %lld = %lld\0A\1B[0m\00", align 1
@.fmt.14 = private unnamed_addr constant [18 x i8] c"BOOM! Game Over.\0A\00", align 1
@.fmt.15 = private unnamed_addr constant [27 x i8] c"Congratulations! You won!\0A\00", align 1
@stdin = external global ptr
@strl.16 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@strl.17 = private unnamed_addr constant [2 x i8] c"W\00", align 1
@strl.18 = private unnamed_addr constant [2 x i8] c"S\00", align 1
@strl.19 = private unnamed_addr constant [2 x i8] c"A\00", align 1
@strl.20 = private unnamed_addr constant [2 x i8] c"D\00", align 1
@strl.21 = private unnamed_addr constant [2 x i8] c"F\00", align 1
@strl.22 = private unnamed_addr constant [2 x i8] c"O\00", align 1
@.fmt.23 = private unnamed_addr constant [6 x i8] c"Open\0A\00", align 1
@strl.24 = private unnamed_addr constant [2 x i8] c"Q\00", align 1
@.fmt.25 = private unnamed_addr constant [7 x i8] c"Quit.\0A\00", align 1

declare i32 @printf(ptr, ...)

declare i32 @strcmp(ptr, ptr)

declare i32 @scanf(ptr, ptr)

define i8 @GetDigit(i32 %0) #0 {
entry:
  %n = alloca i32, align 4
  store i32 %0, ptr %n, align 4
  %n.load = load i32, ptr %n, align 4
  %ieq = icmp eq i32 %n.load, 0
  br i1 %ieq, label %then, label %else

then:                                             ; preds = %entry
  ret i8 46

else:                                             ; preds = %entry
  br label %merge

merge:                                            ; preds = %else
  %n.load1 = load i32, ptr %n, align 4
  %ieq2 = icmp eq i32 %n.load1, 1
  br i1 %ieq2, label %then3, label %else4

then3:                                            ; preds = %merge
  ret i8 49

else4:                                            ; preds = %merge
  br label %merge5

merge5:                                           ; preds = %else4
  %n.load6 = load i32, ptr %n, align 4
  %ieq7 = icmp eq i32 %n.load6, 2
  br i1 %ieq7, label %then8, label %else9

then8:                                            ; preds = %merge5
  ret i8 50

else9:                                            ; preds = %merge5
  br label %merge10

merge10:                                          ; preds = %else9
  %n.load11 = load i32, ptr %n, align 4
  %ieq12 = icmp eq i32 %n.load11, 3
  br i1 %ieq12, label %then13, label %else14

then13:                                           ; preds = %merge10
  ret i8 51

else14:                                           ; preds = %merge10
  br label %merge15

merge15:                                          ; preds = %else14
  %n.load16 = load i32, ptr %n, align 4
  %ieq17 = icmp eq i32 %n.load16, 4
  br i1 %ieq17, label %then18, label %else19

then18:                                           ; preds = %merge15
  ret i8 52

else19:                                           ; preds = %merge15
  br label %merge20

merge20:                                          ; preds = %else19
  %n.load21 = load i32, ptr %n, align 4
  %ieq22 = icmp eq i32 %n.load21, 5
  br i1 %ieq22, label %then23, label %else24

then23:                                           ; preds = %merge20
  ret i8 53

else24:                                           ; preds = %merge20
  br label %merge25

merge25:                                          ; preds = %else24
  %n.load26 = load i32, ptr %n, align 4
  %ieq27 = icmp eq i32 %n.load26, 6
  br i1 %ieq27, label %then28, label %else29

then28:                                           ; preds = %merge25
  ret i8 54

else29:                                           ; preds = %merge25
  br label %merge30

merge30:                                          ; preds = %else29
  %n.load31 = load i32, ptr %n, align 4
  %ieq32 = icmp eq i32 %n.load31, 7
  br i1 %ieq32, label %then33, label %else34

then33:                                           ; preds = %merge30
  ret i8 55

else34:                                           ; preds = %merge30
  br label %merge35

merge35:                                          ; preds = %else34
  %n.load36 = load i32, ptr %n, align 4
  %ieq37 = icmp eq i32 %n.load36, 8
  br i1 %ieq37, label %then38, label %else39

then38:                                           ; preds = %merge35
  ret i8 56

else39:                                           ; preds = %merge35
  br label %merge40

merge40:                                          ; preds = %else39
  ret i8 63
}

define void @Generate(ptr %0, ptr %1, ptr %2, ptr %3) #0 {
entry:
  %board.refptr = alloca ptr, align 8
  store ptr %0, ptr %board.refptr, align 8
  %mines.refptr = alloca ptr, align 8
  store ptr %1, ptr %mines.refptr, align 8
  %opened.refptr = alloca ptr, align 8
  store ptr %2, ptr %opened.refptr, align 8
  %flags.refptr = alloca ptr, align 8
  store ptr %3, ptr %flags.refptr, align 8
  %x = alloca i32, align 4
  store i32 0, ptr %x, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.exit, %entry
  %x.load = load i32, ptr %x, align 4
  %ilt = icmp slt i32 %x.load, 10
  br i1 %ilt, label %while.body, label %while.exit36

while.body:                                       ; preds = %while.cond
  %y = alloca i32, align 4
  store i32 0, ptr %y, align 4
  br label %while.cond1

while.cond1:                                      ; preds = %while.body3, %while.body
  %y.load = load i32, ptr %y, align 4
  %ilt2 = icmp slt i32 %y.load, 20
  br i1 %ilt2, label %while.body3, label %while.exit

while.body3:                                      ; preds = %while.cond1
  %board.ref = load ptr, ptr %board.refptr, align 8
  %x.load4 = load i32, ptr %x, align 4
  %4 = sext i32 %x.load4 to i64
  %arr.load = load %array.array.i8, ptr %board.ref, align 8
  %arr.data = extractvalue %array.array.i8 %arr.load, 1
  %elem.ptr = getelementptr %array.i8, ptr %arr.data, i64 %4
  %y.load5 = load i32, ptr %y, align 4
  %5 = sext i32 %y.load5 to i64
  %arr.load6 = load %array.i8, ptr %elem.ptr, align 8
  %arr.data7 = extractvalue %array.i8 %arr.load6, 1
  %elem.ptr8 = getelementptr i8, ptr %arr.data7, i64 %5
  store i8 35, ptr %elem.ptr8, align 1
  %mines.ref = load ptr, ptr %mines.refptr, align 8
  %x.load9 = load i32, ptr %x, align 4
  %6 = sext i32 %x.load9 to i64
  %arr.load10 = load %array.array.bool, ptr %mines.ref, align 8
  %arr.data11 = extractvalue %array.array.bool %arr.load10, 1
  %elem.ptr12 = getelementptr %array.bool, ptr %arr.data11, i64 %6
  %y.load13 = load i32, ptr %y, align 4
  %7 = sext i32 %y.load13 to i64
  %arr.load14 = load %array.bool, ptr %elem.ptr12, align 8
  %arr.data15 = extractvalue %array.bool %arr.load14, 1
  %elem.ptr16 = getelementptr i1, ptr %arr.data15, i64 %7
  store i1 false, ptr %elem.ptr16, align 1
  %opened.ref = load ptr, ptr %opened.refptr, align 8
  %x.load17 = load i32, ptr %x, align 4
  %8 = sext i32 %x.load17 to i64
  %arr.load18 = load %array.array.bool, ptr %opened.ref, align 8
  %arr.data19 = extractvalue %array.array.bool %arr.load18, 1
  %elem.ptr20 = getelementptr %array.bool, ptr %arr.data19, i64 %8
  %y.load21 = load i32, ptr %y, align 4
  %9 = sext i32 %y.load21 to i64
  %arr.load22 = load %array.bool, ptr %elem.ptr20, align 8
  %arr.data23 = extractvalue %array.bool %arr.load22, 1
  %elem.ptr24 = getelementptr i1, ptr %arr.data23, i64 %9
  store i1 false, ptr %elem.ptr24, align 1
  %flags.ref = load ptr, ptr %flags.refptr, align 8
  %x.load25 = load i32, ptr %x, align 4
  %10 = sext i32 %x.load25 to i64
  %arr.load26 = load %array.array.bool, ptr %flags.ref, align 8
  %arr.data27 = extractvalue %array.array.bool %arr.load26, 1
  %elem.ptr28 = getelementptr %array.bool, ptr %arr.data27, i64 %10
  %y.load29 = load i32, ptr %y, align 4
  %11 = sext i32 %y.load29 to i64
  %arr.load30 = load %array.bool, ptr %elem.ptr28, align 8
  %arr.data31 = extractvalue %array.bool %arr.load30, 1
  %elem.ptr32 = getelementptr i1, ptr %arr.data31, i64 %11
  store i1 false, ptr %elem.ptr32, align 1
  %y.load33 = load i32, ptr %y, align 4
  %addtmp = add i32 %y.load33, 1
  store i32 %addtmp, ptr %y, align 4
  br label %while.cond1

while.exit:                                       ; preds = %while.cond1
  %x.load34 = load i32, ptr %x, align 4
  %addtmp35 = add i32 %x.load34, 1
  store i32 %addtmp35, ptr %x, align 4
  br label %while.cond

while.exit36:                                     ; preds = %while.cond
  ret void
}

define void @PlaceMines(ptr %0) #0 {
entry:
  %mines.refptr = alloca ptr, align 8
  store ptr %0, ptr %mines.refptr, align 8
  %placed = alloca i32, align 4
  store i32 0, ptr %placed, align 4
  br label %while.cond

while.cond:                                       ; preds = %merge, %entry
  %placed.load = load i32, ptr %placed, align 4
  %ilt = icmp slt i32 %placed.load, 40
  br i1 %ilt, label %while.body, label %while.exit

while.body:                                       ; preds = %while.cond
  %rx = alloca i32, align 4
  %rand.val = call i32 @rand()
  %rand.f = sitofp i32 %rand.val to double
  %random.f = fdiv double %rand.f, 0x41DFFFFFFFC00000
  %fmul = fmul double %random.f, 1.000000e+01
  %fptosi = fptosi double %fmul to i32
  store i32 %fptosi, ptr %rx, align 4
  %ry = alloca i32, align 4
  %rand.val1 = call i32 @rand()
  %rand.f2 = sitofp i32 %rand.val1 to double
  %random.f3 = fdiv double %rand.f2, 0x41DFFFFFFFC00000
  %fmul4 = fmul double %random.f3, 2.000000e+01
  %fptosi5 = fptosi double %fmul4 to i32
  store i32 %fptosi5, ptr %ry, align 4
  %1 = load ptr, ptr %mines.refptr, align 8
  %rx.load = load i32, ptr %rx, align 4
  %2 = sext i32 %rx.load to i64
  %arr.load = load %array.array.bool, ptr %1, align 8
  %arr.data = extractvalue %array.array.bool %arr.load, 1
  %elem.ptr = getelementptr %array.bool, ptr %arr.data, i64 %2
  %ry.load = load i32, ptr %ry, align 4
  %3 = sext i32 %ry.load to i64
  %arr.load6 = load %array.bool, ptr %elem.ptr, align 8
  %arr.data7 = extractvalue %array.bool %arr.load6, 1
  %elem.ptr8 = getelementptr i1, ptr %arr.data7, i64 %3
  %elem = load i1, ptr %elem.ptr8, align 1
  %lnot = xor i1 %elem, true
  br i1 %lnot, label %then, label %else

then:                                             ; preds = %while.body
  %mines.ref = load ptr, ptr %mines.refptr, align 8
  %rx.load9 = load i32, ptr %rx, align 4
  %4 = sext i32 %rx.load9 to i64
  %arr.load10 = load %array.array.bool, ptr %mines.ref, align 8
  %arr.data11 = extractvalue %array.array.bool %arr.load10, 1
  %elem.ptr12 = getelementptr %array.bool, ptr %arr.data11, i64 %4
  %ry.load13 = load i32, ptr %ry, align 4
  %5 = sext i32 %ry.load13 to i64
  %arr.load14 = load %array.bool, ptr %elem.ptr12, align 8
  %arr.data15 = extractvalue %array.bool %arr.load14, 1
  %elem.ptr16 = getelementptr i1, ptr %arr.data15, i64 %5
  store i1 true, ptr %elem.ptr16, align 1
  %placed.load17 = load i32, ptr %placed, align 4
  %addtmp = add i32 %placed.load17, 1
  store i32 %addtmp, ptr %placed, align 4
  br label %merge

else:                                             ; preds = %while.body
  br label %merge

merge:                                            ; preds = %else, %then
  br label %while.cond

while.exit:                                       ; preds = %while.cond
  ret void
}

define i32 @Count(ptr %0, i32 %1, i32 %2) #0 {
entry:
  %mines.refptr = alloca ptr, align 8
  store ptr %0, ptr %mines.refptr, align 8
  %x = alloca i32, align 4
  store i32 %1, ptr %x, align 4
  %y = alloca i32, align 4
  store i32 %2, ptr %y, align 4
  %count = alloca i32, align 4
  store i32 0, ptr %count, align 4
  %dx = alloca i32, align 4
  store i32 -1, ptr %dx, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.exit, %entry
  %dx.load = load i32, ptr %dx, align 4
  %ile = icmp sle i32 %dx.load, 1
  br i1 %ile, label %while.body, label %while.exit41

while.body:                                       ; preds = %while.cond
  %dy = alloca i32, align 4
  store i32 -1, ptr %dy, align 4
  br label %while.cond1

while.cond1:                                      ; preds = %merge38, %while.body
  %dy.load = load i32, ptr %dy, align 4
  %ile2 = icmp sle i32 %dy.load, 1
  br i1 %ile2, label %while.body3, label %while.exit

while.body3:                                      ; preds = %while.cond1
  %dx.load4 = load i32, ptr %dx, align 4
  %ieq = icmp eq i32 %dx.load4, 0
  %dy.load5 = load i32, ptr %dy, align 4
  %ieq6 = icmp eq i32 %dy.load5, 0
  %l.bool = icmp ne i1 %ieq, false
  %r.bool = icmp ne i1 %ieq6, false
  %land = and i1 %l.bool, %r.bool
  br i1 %land, label %then, label %else

then:                                             ; preds = %while.body3
  %dy.load7 = load i32, ptr %dy, align 4
  %addtmp = add i32 %dy.load7, 1
  store i32 %addtmp, ptr %dy, align 4
  br label %merge38

else:                                             ; preds = %while.body3
  %nx = alloca i32, align 4
  %x.load = load i32, ptr %x, align 4
  %dx.load8 = load i32, ptr %dx, align 4
  %addtmp9 = add i32 %x.load, %dx.load8
  store i32 %addtmp9, ptr %nx, align 4
  %ny = alloca i32, align 4
  %y.load = load i32, ptr %y, align 4
  %dy.load10 = load i32, ptr %dy, align 4
  %addtmp11 = add i32 %y.load, %dy.load10
  store i32 %addtmp11, ptr %ny, align 4
  %nx.load = load i32, ptr %nx, align 4
  %ige = icmp sge i32 %nx.load, 0
  %nx.load12 = load i32, ptr %nx, align 4
  %ilt = icmp slt i32 %nx.load12, 10
  %l.bool13 = icmp ne i1 %ige, false
  %r.bool14 = icmp ne i1 %ilt, false
  %land15 = and i1 %l.bool13, %r.bool14
  %ny.load = load i32, ptr %ny, align 4
  %ige16 = icmp sge i32 %ny.load, 0
  %l.bool17 = icmp ne i1 %land15, false
  %r.bool18 = icmp ne i1 %ige16, false
  %land19 = and i1 %l.bool17, %r.bool18
  %ny.load20 = load i32, ptr %ny, align 4
  %ilt21 = icmp slt i32 %ny.load20, 20
  %l.bool22 = icmp ne i1 %land19, false
  %r.bool23 = icmp ne i1 %ilt21, false
  %land24 = and i1 %l.bool22, %r.bool23
  br i1 %land24, label %then25, label %else34

then25:                                           ; preds = %else
  %3 = load ptr, ptr %mines.refptr, align 8
  %nx.load26 = load i32, ptr %nx, align 4
  %4 = sext i32 %nx.load26 to i64
  %arr.load = load %array.array.bool, ptr %3, align 8
  %arr.data = extractvalue %array.array.bool %arr.load, 1
  %elem.ptr = getelementptr %array.bool, ptr %arr.data, i64 %4
  %ny.load27 = load i32, ptr %ny, align 4
  %5 = sext i32 %ny.load27 to i64
  %arr.load28 = load %array.bool, ptr %elem.ptr, align 8
  %arr.data29 = extractvalue %array.bool %arr.load28, 1
  %elem.ptr30 = getelementptr i1, ptr %arr.data29, i64 %5
  %elem = load i1, ptr %elem.ptr30, align 1
  br i1 %elem, label %then31, label %else33

then31:                                           ; preds = %then25
  %count.load = load i32, ptr %count, align 4
  %addtmp32 = add i32 %count.load, 1
  store i32 %addtmp32, ptr %count, align 4
  br label %merge

else33:                                           ; preds = %then25
  br label %merge

merge:                                            ; preds = %else33, %then31
  br label %merge35

else34:                                           ; preds = %else
  br label %merge35

merge35:                                          ; preds = %else34, %merge
  %dy.load36 = load i32, ptr %dy, align 4
  %addtmp37 = add i32 %dy.load36, 1
  store i32 %addtmp37, ptr %dy, align 4
  br label %merge38

merge38:                                          ; preds = %merge35, %then
  br label %while.cond1

while.exit:                                       ; preds = %while.cond1
  %dx.load39 = load i32, ptr %dx, align 4
  %addtmp40 = add i32 %dx.load39, 1
  store i32 %addtmp40, ptr %dx, align 4
  br label %while.cond

while.exit41:                                     ; preds = %while.cond
  %count.load42 = load i32, ptr %count, align 4
  ret i32 %count.load42
}

define void @FloodReveal(ptr %0, ptr %1, ptr %2, i32 %3, i32 %4) #0 {
entry:
  %board.refptr = alloca ptr, align 8
  store ptr %0, ptr %board.refptr, align 8
  %mines.refptr = alloca ptr, align 8
  store ptr %1, ptr %mines.refptr, align 8
  %opened.refptr = alloca ptr, align 8
  store ptr %2, ptr %opened.refptr, align 8
  %x = alloca i32, align 4
  store i32 %3, ptr %x, align 4
  %y = alloca i32, align 4
  store i32 %4, ptr %y, align 4
  %x.load = load i32, ptr %x, align 4
  %ilt = icmp slt i32 %x.load, 0
  %x.load1 = load i32, ptr %x, align 4
  %ige = icmp sge i32 %x.load1, 10
  %l.bool = icmp ne i1 %ilt, false
  %r.bool = icmp ne i1 %ige, false
  %lor = or i1 %l.bool, %r.bool
  %y.load = load i32, ptr %y, align 4
  %ilt2 = icmp slt i32 %y.load, 0
  %l.bool3 = icmp ne i1 %lor, false
  %r.bool4 = icmp ne i1 %ilt2, false
  %lor5 = or i1 %l.bool3, %r.bool4
  %y.load6 = load i32, ptr %y, align 4
  %ige7 = icmp sge i32 %y.load6, 20
  %l.bool8 = icmp ne i1 %lor5, false
  %r.bool9 = icmp ne i1 %ige7, false
  %lor10 = or i1 %l.bool8, %r.bool9
  br i1 %lor10, label %then, label %else

then:                                             ; preds = %entry
  ret void

else:                                             ; preds = %entry
  br label %merge

merge:                                            ; preds = %else
  %case = alloca i1, align 1
  %5 = load ptr, ptr %opened.refptr, align 8
  %x.load11 = load i32, ptr %x, align 4
  %6 = sext i32 %x.load11 to i64
  %arr.load = load %array.array.bool, ptr %5, align 8
  %arr.data = extractvalue %array.array.bool %arr.load, 1
  %elem.ptr = getelementptr %array.bool, ptr %arr.data, i64 %6
  %y.load12 = load i32, ptr %y, align 4
  %7 = sext i32 %y.load12 to i64
  %arr.load13 = load %array.bool, ptr %elem.ptr, align 8
  %arr.data14 = extractvalue %array.bool %arr.load13, 1
  %elem.ptr15 = getelementptr i1, ptr %arr.data14, i64 %7
  %elem = load i1, ptr %elem.ptr15, align 1
  store i1 %elem, ptr %case, align 1
  %case.load = load i1, ptr %case, align 1
  br i1 %case.load, label %then16, label %else17

then16:                                           ; preds = %merge
  ret void

else17:                                           ; preds = %merge
  br label %merge18

merge18:                                          ; preds = %else17
  %opened.ref = load ptr, ptr %opened.refptr, align 8
  %x.load19 = load i32, ptr %x, align 4
  %8 = sext i32 %x.load19 to i64
  %arr.load20 = load %array.array.bool, ptr %opened.ref, align 8
  %arr.data21 = extractvalue %array.array.bool %arr.load20, 1
  %elem.ptr22 = getelementptr %array.bool, ptr %arr.data21, i64 %8
  %y.load23 = load i32, ptr %y, align 4
  %9 = sext i32 %y.load23 to i64
  %arr.load24 = load %array.bool, ptr %elem.ptr22, align 8
  %arr.data25 = extractvalue %array.bool %arr.load24, 1
  %elem.ptr26 = getelementptr i1, ptr %arr.data25, i64 %9
  store i1 true, ptr %elem.ptr26, align 1
  %10 = load ptr, ptr %mines.refptr, align 8
  %x.load27 = load i32, ptr %x, align 4
  %11 = sext i32 %x.load27 to i64
  %arr.load28 = load %array.array.bool, ptr %10, align 8
  %arr.data29 = extractvalue %array.array.bool %arr.load28, 1
  %elem.ptr30 = getelementptr %array.bool, ptr %arr.data29, i64 %11
  %y.load31 = load i32, ptr %y, align 4
  %12 = sext i32 %y.load31 to i64
  %arr.load32 = load %array.bool, ptr %elem.ptr30, align 8
  %arr.data33 = extractvalue %array.bool %arr.load32, 1
  %elem.ptr34 = getelementptr i1, ptr %arr.data33, i64 %12
  %elem35 = load i1, ptr %elem.ptr34, align 1
  br i1 %elem35, label %then36, label %else45

then36:                                           ; preds = %merge18
  %board.ref = load ptr, ptr %board.refptr, align 8
  %x.load37 = load i32, ptr %x, align 4
  %13 = sext i32 %x.load37 to i64
  %arr.load38 = load %array.array.i8, ptr %board.ref, align 8
  %arr.data39 = extractvalue %array.array.i8 %arr.load38, 1
  %elem.ptr40 = getelementptr %array.i8, ptr %arr.data39, i64 %13
  %y.load41 = load i32, ptr %y, align 4
  %14 = sext i32 %y.load41 to i64
  %arr.load42 = load %array.i8, ptr %elem.ptr40, align 8
  %arr.data43 = extractvalue %array.i8 %arr.load42, 1
  %elem.ptr44 = getelementptr i8, ptr %arr.data43, i64 %14
  store i8 42, ptr %elem.ptr44, align 1
  ret void

else45:                                           ; preds = %merge18
  br label %merge46

merge46:                                          ; preds = %else45
  %n = alloca i32, align 4
  %mines.fwd = load ptr, ptr %mines.refptr, align 8
  %x.load47 = load i32, ptr %x, align 4
  %y.load48 = load i32, ptr %y, align 4
  %call = call i32 @Count(ptr %mines.fwd, i32 %x.load47, i32 %y.load48)
  store i32 %call, ptr %n, align 4
  %board.ref49 = load ptr, ptr %board.refptr, align 8
  %x.load50 = load i32, ptr %x, align 4
  %15 = sext i32 %x.load50 to i64
  %arr.load51 = load %array.array.i8, ptr %board.ref49, align 8
  %arr.data52 = extractvalue %array.array.i8 %arr.load51, 1
  %elem.ptr53 = getelementptr %array.i8, ptr %arr.data52, i64 %15
  %y.load54 = load i32, ptr %y, align 4
  %16 = sext i32 %y.load54 to i64
  %arr.load55 = load %array.i8, ptr %elem.ptr53, align 8
  %arr.data56 = extractvalue %array.i8 %arr.load55, 1
  %elem.ptr57 = getelementptr i8, ptr %arr.data56, i64 %16
  %n.load = load i32, ptr %n, align 4
  %call58 = call i8 @GetDigit(i32 %n.load)
  store i8 %call58, ptr %elem.ptr57, align 1
  %n.load59 = load i32, ptr %n, align 4
  %ieq = icmp eq i32 %n.load59, 0
  br i1 %ieq, label %then60, label %else84

then60:                                           ; preds = %merge46
  %dx = alloca i32, align 4
  store i32 -1, ptr %dx, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.exit, %then60
  %dx.load = load i32, ptr %dx, align 4
  %ile = icmp sle i32 %dx.load, 1
  br i1 %ile, label %while.body, label %while.exit83

while.body:                                       ; preds = %while.cond
  %dy = alloca i32, align 4
  store i32 -1, ptr %dy, align 4
  br label %while.cond61

while.cond61:                                     ; preds = %merge78, %while.body
  %dy.load = load i32, ptr %dy, align 4
  %ile62 = icmp sle i32 %dy.load, 1
  br i1 %ile62, label %while.body63, label %while.exit

while.body63:                                     ; preds = %while.cond61
  %dx.load64 = load i32, ptr %dx, align 4
  %ine = icmp ne i32 %dx.load64, 0
  %dy.load65 = load i32, ptr %dy, align 4
  %ine66 = icmp ne i32 %dy.load65, 0
  %l.bool67 = icmp ne i1 %ine, false
  %r.bool68 = icmp ne i1 %ine66, false
  %lor69 = or i1 %l.bool67, %r.bool68
  br i1 %lor69, label %then70, label %else77

then70:                                           ; preds = %while.body63
  %board.fwd = load ptr, ptr %board.refptr, align 8
  %mines.fwd71 = load ptr, ptr %mines.refptr, align 8
  %opened.fwd = load ptr, ptr %opened.refptr, align 8
  %x.load72 = load i32, ptr %x, align 4
  %dx.load73 = load i32, ptr %dx, align 4
  %addtmp = add i32 %x.load72, %dx.load73
  %y.load74 = load i32, ptr %y, align 4
  %dy.load75 = load i32, ptr %dy, align 4
  %addtmp76 = add i32 %y.load74, %dy.load75
  call void @FloodReveal(ptr %board.fwd, ptr %mines.fwd71, ptr %opened.fwd, i32 %addtmp, i32 %addtmp76)
  br label %merge78

else77:                                           ; preds = %while.body63
  br label %merge78

merge78:                                          ; preds = %else77, %then70
  %dy.load79 = load i32, ptr %dy, align 4
  %addtmp80 = add i32 %dy.load79, 1
  store i32 %addtmp80, ptr %dy, align 4
  br label %while.cond61

while.exit:                                       ; preds = %while.cond61
  %dx.load81 = load i32, ptr %dx, align 4
  %addtmp82 = add i32 %dx.load81, 1
  store i32 %addtmp82, ptr %dx, align 4
  br label %while.cond

while.exit83:                                     ; preds = %while.cond
  br label %merge85

else84:                                           ; preds = %merge46
  br label %merge85

merge85:                                          ; preds = %else84, %while.exit83
  ret void
}

define void @Open(ptr %0, ptr %1, ptr %2, i32 %3, i32 %4) #0 {
entry:
  %board.refptr = alloca ptr, align 8
  store ptr %0, ptr %board.refptr, align 8
  %mines.refptr = alloca ptr, align 8
  store ptr %1, ptr %mines.refptr, align 8
  %opened.refptr = alloca ptr, align 8
  store ptr %2, ptr %opened.refptr, align 8
  %x = alloca i32, align 4
  store i32 %3, ptr %x, align 4
  %y = alloca i32, align 4
  store i32 %4, ptr %y, align 4
  %5 = load ptr, ptr %opened.refptr, align 8
  %x.load = load i32, ptr %x, align 4
  %6 = sext i32 %x.load to i64
  %arr.load = load %array.array.bool, ptr %5, align 8
  %arr.data = extractvalue %array.array.bool %arr.load, 1
  %elem.ptr = getelementptr %array.bool, ptr %arr.data, i64 %6
  %y.load = load i32, ptr %y, align 4
  %7 = sext i32 %y.load to i64
  %arr.load1 = load %array.bool, ptr %elem.ptr, align 8
  %arr.data2 = extractvalue %array.bool %arr.load1, 1
  %elem.ptr3 = getelementptr i1, ptr %arr.data2, i64 %7
  %elem = load i1, ptr %elem.ptr3, align 1
  br i1 %elem, label %then, label %else

then:                                             ; preds = %entry
  ret void

else:                                             ; preds = %entry
  br label %merge

merge:                                            ; preds = %else
  %8 = load ptr, ptr %mines.refptr, align 8
  %x.load4 = load i32, ptr %x, align 4
  %9 = sext i32 %x.load4 to i64
  %arr.load5 = load %array.array.bool, ptr %8, align 8
  %arr.data6 = extractvalue %array.array.bool %arr.load5, 1
  %elem.ptr7 = getelementptr %array.bool, ptr %arr.data6, i64 %9
  %y.load8 = load i32, ptr %y, align 4
  %10 = sext i32 %y.load8 to i64
  %arr.load9 = load %array.bool, ptr %elem.ptr7, align 8
  %arr.data10 = extractvalue %array.bool %arr.load9, 1
  %elem.ptr11 = getelementptr i1, ptr %arr.data10, i64 %10
  %elem12 = load i1, ptr %elem.ptr11, align 1
  br i1 %elem12, label %then13, label %else30

then13:                                           ; preds = %merge
  %board.ref = load ptr, ptr %board.refptr, align 8
  %x.load14 = load i32, ptr %x, align 4
  %11 = sext i32 %x.load14 to i64
  %arr.load15 = load %array.array.i8, ptr %board.ref, align 8
  %arr.data16 = extractvalue %array.array.i8 %arr.load15, 1
  %elem.ptr17 = getelementptr %array.i8, ptr %arr.data16, i64 %11
  %y.load18 = load i32, ptr %y, align 4
  %12 = sext i32 %y.load18 to i64
  %arr.load19 = load %array.i8, ptr %elem.ptr17, align 8
  %arr.data20 = extractvalue %array.i8 %arr.load19, 1
  %elem.ptr21 = getelementptr i8, ptr %arr.data20, i64 %12
  store i8 42, ptr %elem.ptr21, align 1
  %opened.ref = load ptr, ptr %opened.refptr, align 8
  %x.load22 = load i32, ptr %x, align 4
  %13 = sext i32 %x.load22 to i64
  %arr.load23 = load %array.array.bool, ptr %opened.ref, align 8
  %arr.data24 = extractvalue %array.array.bool %arr.load23, 1
  %elem.ptr25 = getelementptr %array.bool, ptr %arr.data24, i64 %13
  %y.load26 = load i32, ptr %y, align 4
  %14 = sext i32 %y.load26 to i64
  %arr.load27 = load %array.bool, ptr %elem.ptr25, align 8
  %arr.data28 = extractvalue %array.bool %arr.load27, 1
  %elem.ptr29 = getelementptr i1, ptr %arr.data28, i64 %14
  store i1 true, ptr %elem.ptr29, align 1
  ret void

else30:                                           ; preds = %merge
  br label %merge31

merge31:                                          ; preds = %else30
  %case = alloca i1, align 1
  %15 = load ptr, ptr %opened.refptr, align 8
  %x.load32 = load i32, ptr %x, align 4
  %16 = sext i32 %x.load32 to i64
  %arr.load33 = load %array.array.bool, ptr %15, align 8
  %arr.data34 = extractvalue %array.array.bool %arr.load33, 1
  %elem.ptr35 = getelementptr %array.bool, ptr %arr.data34, i64 %16
  %y.load36 = load i32, ptr %y, align 4
  %17 = sext i32 %y.load36 to i64
  %arr.load37 = load %array.bool, ptr %elem.ptr35, align 8
  %arr.data38 = extractvalue %array.bool %arr.load37, 1
  %elem.ptr39 = getelementptr i1, ptr %arr.data38, i64 %17
  %elem40 = load i1, ptr %elem.ptr39, align 1
  store i1 %elem40, ptr %case, align 1
  %board.fwd = load ptr, ptr %board.refptr, align 8
  %mines.fwd = load ptr, ptr %mines.refptr, align 8
  %opened.fwd = load ptr, ptr %opened.refptr, align 8
  %x.load41 = load i32, ptr %x, align 4
  %y.load42 = load i32, ptr %y, align 4
  call void @FloodReveal(ptr %board.fwd, ptr %mines.fwd, ptr %opened.fwd, i32 %x.load41, i32 %y.load42)
  ret void
}

define i1 @HasWon(ptr %0, ptr %1) #0 {
entry:
  %mines.refptr = alloca ptr, align 8
  store ptr %0, ptr %mines.refptr, align 8
  %opened.refptr = alloca ptr, align 8
  store ptr %1, ptr %opened.refptr, align 8
  %x = alloca i32, align 4
  store i32 0, ptr %x, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.exit, %entry
  %x.load = load i32, ptr %x, align 4
  %ilt = icmp slt i32 %x.load, 10
  br i1 %ilt, label %while.body, label %while.exit22

while.body:                                       ; preds = %while.cond
  %y = alloca i32, align 4
  store i32 0, ptr %y, align 4
  br label %while.cond1

while.cond1:                                      ; preds = %merge, %while.body
  %y.load = load i32, ptr %y, align 4
  %ilt2 = icmp slt i32 %y.load, 20
  br i1 %ilt2, label %while.body3, label %while.exit

while.body3:                                      ; preds = %while.cond1
  %2 = load ptr, ptr %mines.refptr, align 8
  %x.load4 = load i32, ptr %x, align 4
  %3 = sext i32 %x.load4 to i64
  %arr.load = load %array.array.bool, ptr %2, align 8
  %arr.data = extractvalue %array.array.bool %arr.load, 1
  %elem.ptr = getelementptr %array.bool, ptr %arr.data, i64 %3
  %y.load5 = load i32, ptr %y, align 4
  %4 = sext i32 %y.load5 to i64
  %arr.load6 = load %array.bool, ptr %elem.ptr, align 8
  %arr.data7 = extractvalue %array.bool %arr.load6, 1
  %elem.ptr8 = getelementptr i1, ptr %arr.data7, i64 %4
  %elem = load i1, ptr %elem.ptr8, align 1
  %lnot = xor i1 %elem, true
  %5 = load ptr, ptr %opened.refptr, align 8
  %x.load9 = load i32, ptr %x, align 4
  %6 = sext i32 %x.load9 to i64
  %arr.load10 = load %array.array.bool, ptr %5, align 8
  %arr.data11 = extractvalue %array.array.bool %arr.load10, 1
  %elem.ptr12 = getelementptr %array.bool, ptr %arr.data11, i64 %6
  %y.load13 = load i32, ptr %y, align 4
  %7 = sext i32 %y.load13 to i64
  %arr.load14 = load %array.bool, ptr %elem.ptr12, align 8
  %arr.data15 = extractvalue %array.bool %arr.load14, 1
  %elem.ptr16 = getelementptr i1, ptr %arr.data15, i64 %7
  %elem17 = load i1, ptr %elem.ptr16, align 1
  %lnot18 = xor i1 %elem17, true
  %l.bool = icmp ne i1 %lnot, false
  %r.bool = icmp ne i1 %lnot18, false
  %land = and i1 %l.bool, %r.bool
  br i1 %land, label %then, label %else

then:                                             ; preds = %while.body3
  ret i1 false

else:                                             ; preds = %while.body3
  br label %merge

merge:                                            ; preds = %else
  %y.load19 = load i32, ptr %y, align 4
  %addtmp = add i32 %y.load19, 1
  store i32 %addtmp, ptr %y, align 4
  br label %while.cond1

while.exit:                                       ; preds = %while.cond1
  %x.load20 = load i32, ptr %x, align 4
  %addtmp21 = add i32 %x.load20, 1
  store i32 %addtmp21, ptr %x, align 4
  br label %while.cond

while.exit22:                                     ; preds = %while.cond
  ret i1 true
}

define void @Draw(ptr %0, ptr %1, ptr %2, i32 %3, i32 %4) #0 {
entry:
  %board.refptr = alloca ptr, align 8
  store ptr %0, ptr %board.refptr, align 8
  %opened.refptr = alloca ptr, align 8
  store ptr %1, ptr %opened.refptr, align 8
  %flags.refptr = alloca ptr, align 8
  store ptr %2, ptr %flags.refptr, align 8
  %cx = alloca i32, align 4
  store i32 %3, ptr %cx, align 4
  %cy = alloca i32, align 4
  store i32 %4, ptr %cy, align 4
  %wall = alloca %string, align 8
  %str.alloc = call ptr @malloc(i64 1)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc, ptr @strl, i64 0, i1 false)
  %5 = getelementptr i8, ptr %str.alloc, i64 0
  store i8 0, ptr %5, align 1
  %string = alloca %string, align 8
  %6 = getelementptr inbounds nuw %string, ptr %string, i32 0, i32 0
  store ptr %str.alloc, ptr %6, align 8
  %7 = getelementptr inbounds nuw %string, ptr %string, i32 0, i32 1
  store i64 0, ptr %7, align 4
  %8 = getelementptr inbounds nuw %string, ptr %string, i32 0, i32 2
  store i64 1, ptr %8, align 4
  %str.alloc1 = call ptr @malloc(i64 1)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc1, ptr @strl.1, i64 0, i1 false)
  %9 = getelementptr i8, ptr %str.alloc1, i64 0
  store i8 0, ptr %9, align 1
  %string2 = alloca %string, align 8
  %10 = getelementptr inbounds nuw %string, ptr %string2, i32 0, i32 0
  store ptr %str.alloc1, ptr %10, align 8
  %11 = getelementptr inbounds nuw %string, ptr %string2, i32 0, i32 1
  store i64 0, ptr %11, align 4
  %12 = getelementptr inbounds nuw %string, ptr %string2, i32 0, i32 2
  store i64 1, ptr %12, align 4
  %ls = load %string, ptr %string, align 8
  %rs = load %string, ptr %string2, align 8
  %ld = extractvalue %string %ls, 0
  %ll = extractvalue %string %ls, 1
  %rd = extractvalue %string %rs, 0
  %rl = extractvalue %string %rs, 1
  %tlen = add i64 %ll, %rl
  %tcap = add i64 %tlen, 1
  %cat.alloc = call ptr @malloc(i64 %tcap)
  call void @llvm.memcpy.p0.p0.i64(ptr %cat.alloc, ptr %ld, i64 %ll, i1 false)
  %13 = getelementptr i8, ptr %cat.alloc, i64 %ll
  call void @llvm.memcpy.p0.p0.i64(ptr %13, ptr %rd, i64 %rl, i1 false)
  %14 = getelementptr i8, ptr %cat.alloc, i64 %tlen
  store i8 0, ptr %14, align 1
  %cat.res = alloca %string, align 8
  %15 = getelementptr inbounds nuw %string, ptr %cat.res, i32 0, i32 0
  store ptr %cat.alloc, ptr %15, align 8
  %16 = getelementptr inbounds nuw %string, ptr %cat.res, i32 0, i32 1
  store i64 %tlen, ptr %16, align 4
  %17 = getelementptr inbounds nuw %string, ptr %cat.res, i32 0, i32 2
  store i64 %tcap, ptr %17, align 4
  %18 = load %string, ptr %cat.res, align 8
  store %string %18, ptr %wall, align 8
  %19 = load %string, ptr %string2, align 8
  %empty.data = extractvalue %string %19, 0
  call void @free(ptr %empty.data)
  %cnt = alloca i32, align 4
  store i32 0, ptr %cnt, align 4
  br label %while.cond

while.cond:                                       ; preds = %str.skip, %entry
  %cnt.load = load i32, ptr %cnt, align 4
  %ilt = icmp slt i32 %cnt.load, 20
  br i1 %ilt, label %while.body, label %while.exit

while.body:                                       ; preds = %while.cond
  %str.alloc3 = call ptr @malloc(i64 4)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc3, ptr @strl.2, i64 3, i1 false)
  %20 = getelementptr i8, ptr %str.alloc3, i64 3
  store i8 0, ptr %20, align 1
  %string4 = alloca %string, align 8
  %21 = getelementptr inbounds nuw %string, ptr %string4, i32 0, i32 0
  store ptr %str.alloc3, ptr %21, align 8
  %22 = getelementptr inbounds nuw %string, ptr %string4, i32 0, i32 1
  store i64 3, ptr %22, align 4
  %23 = getelementptr inbounds nuw %string, ptr %string4, i32 0, i32 2
  store i64 4, ptr %23, align 4
  %ls5 = load %string, ptr %wall, align 8
  %rs6 = load %string, ptr %string4, align 8
  %ld7 = extractvalue %string %ls5, 0
  %ll8 = extractvalue %string %ls5, 1
  %rd9 = extractvalue %string %rs6, 0
  %rl10 = extractvalue %string %rs6, 1
  %tlen11 = add i64 %ll8, %rl10
  %tcap12 = add i64 %tlen11, 1
  %cat.alloc13 = call ptr @malloc(i64 %tcap12)
  call void @llvm.memcpy.p0.p0.i64(ptr %cat.alloc13, ptr %ld7, i64 %ll8, i1 false)
  %24 = getelementptr i8, ptr %cat.alloc13, i64 %ll8
  call void @llvm.memcpy.p0.p0.i64(ptr %24, ptr %rd9, i64 %rl10, i1 false)
  %25 = getelementptr i8, ptr %cat.alloc13, i64 %tlen11
  store i8 0, ptr %25, align 1
  %cat.res14 = alloca %string, align 8
  %26 = getelementptr inbounds nuw %string, ptr %cat.res14, i32 0, i32 0
  store ptr %cat.alloc13, ptr %26, align 8
  %27 = getelementptr inbounds nuw %string, ptr %cat.res14, i32 0, i32 1
  store i64 %tlen11, ptr %27, align 4
  %28 = getelementptr inbounds nuw %string, ptr %cat.res14, i32 0, i32 2
  store i64 %tcap12, ptr %28, align 4
  %wall.old = load %string, ptr %wall, align 8
  %old.data = extractvalue %string %wall.old, 0
  call void @free(ptr %old.data)
  %29 = load %string, ptr %cat.res14, align 8
  store %string %29, ptr %wall, align 8
  %null.data.ptr = getelementptr inbounds nuw %string, ptr %cat.res14, i32 0, i32 0
  store ptr null, ptr %null.data.ptr, align 8
  %cnt.load15 = load i32, ptr %cnt, align 4
  %addtmp = add i32 %cnt.load15, 1
  store i32 %addtmp, ptr %cnt, align 4
  %30 = load %string, ptr %string4, align 8
  %31 = extractvalue %string %30, 0
  %is.null = icmp eq ptr %31, null
  br i1 %is.null, label %str.skip, label %str.free

str.free:                                         ; preds = %while.body
  call void @free(ptr %31)
  br label %str.skip

str.skip:                                         ; preds = %str.free, %while.body
  br label %while.cond

while.exit:                                       ; preds = %while.cond
  %32 = load %string, ptr %wall, align 8
  %33 = extractvalue %string %32, 0
  %printf.ret = call i32 (ptr, ...) @printf(ptr @.fmt, ptr %33)
  %x = alloca i32, align 4
  store i32 0, ptr %x, align 4
  br label %while.cond16

while.cond16:                                     ; preds = %while.exit76, %while.exit
  %x.load = load i32, ptr %x, align 4
  %ilt17 = icmp slt i32 %x.load, 10
  br i1 %ilt17, label %while.body18, label %while.exit80

while.body18:                                     ; preds = %while.cond16
  %printf.ret19 = call i32 (ptr, ...) @printf(ptr @.fmt.3)
  %y = alloca i32, align 4
  store i32 0, ptr %y, align 4
  br label %while.cond20

while.cond20:                                     ; preds = %merge73, %while.body18
  %y.load = load i32, ptr %y, align 4
  %ilt21 = icmp slt i32 %y.load, 20
  br i1 %ilt21, label %while.body22, label %while.exit76

while.body22:                                     ; preds = %while.cond20
  %t = alloca i8, align 1
  %34 = load ptr, ptr %board.refptr, align 8
  %x.load23 = load i32, ptr %x, align 4
  %35 = sext i32 %x.load23 to i64
  %arr.load = load %array.array.i8, ptr %34, align 8
  %arr.data = extractvalue %array.array.i8 %arr.load, 1
  %elem.ptr = getelementptr %array.i8, ptr %arr.data, i64 %35
  %y.load24 = load i32, ptr %y, align 4
  %36 = sext i32 %y.load24 to i64
  %arr.load25 = load %array.i8, ptr %elem.ptr, align 8
  %arr.data26 = extractvalue %array.i8 %arr.load25, 1
  %elem.ptr27 = getelementptr i8, ptr %arr.data26, i64 %36
  %elem = load i8, ptr %elem.ptr27, align 1
  store i8 %elem, ptr %t, align 1
  %cursor = alloca i1, align 1
  %x.load28 = load i32, ptr %x, align 4
  %cx.load = load i32, ptr %cx, align 4
  %ieq = icmp eq i32 %x.load28, %cx.load
  %y.load29 = load i32, ptr %y, align 4
  %cy.load = load i32, ptr %cy, align 4
  %ieq30 = icmp eq i32 %y.load29, %cy.load
  %l.bool = icmp ne i1 %ieq, false
  %r.bool = icmp ne i1 %ieq30, false
  %land = and i1 %l.bool, %r.bool
  store i1 %land, ptr %cursor, align 1
  %visible = alloca i1, align 1
  %37 = load ptr, ptr %opened.refptr, align 8
  %x.load31 = load i32, ptr %x, align 4
  %38 = sext i32 %x.load31 to i64
  %arr.load32 = load %array.array.bool, ptr %37, align 8
  %arr.data33 = extractvalue %array.array.bool %arr.load32, 1
  %elem.ptr34 = getelementptr %array.bool, ptr %arr.data33, i64 %38
  %y.load35 = load i32, ptr %y, align 4
  %39 = sext i32 %y.load35 to i64
  %arr.load36 = load %array.bool, ptr %elem.ptr34, align 8
  %arr.data37 = extractvalue %array.bool %arr.load36, 1
  %elem.ptr38 = getelementptr i1, ptr %arr.data37, i64 %39
  %elem39 = load i1, ptr %elem.ptr38, align 1
  %cursor.load = load i1, ptr %cursor, align 1
  %l.bool40 = icmp ne i1 %elem39, false
  %r.bool41 = icmp ne i1 %cursor.load, false
  %lor = or i1 %l.bool40, %r.bool41
  store i1 %lor, ptr %visible, align 1
  %cursor.load42 = load i1, ptr %cursor, align 1
  br i1 %cursor.load42, label %then, label %else

then:                                             ; preds = %while.body22
  %printf.ret43 = call i32 (ptr, ...) @printf(ptr @.fmt.4)
  br label %merge73

else:                                             ; preds = %while.body22
  %40 = load ptr, ptr %flags.refptr, align 8
  %x.load44 = load i32, ptr %x, align 4
  %41 = sext i32 %x.load44 to i64
  %arr.load45 = load %array.array.bool, ptr %40, align 8
  %arr.data46 = extractvalue %array.array.bool %arr.load45, 1
  %elem.ptr47 = getelementptr %array.bool, ptr %arr.data46, i64 %41
  %y.load48 = load i32, ptr %y, align 4
  %42 = sext i32 %y.load48 to i64
  %arr.load49 = load %array.bool, ptr %elem.ptr47, align 8
  %arr.data50 = extractvalue %array.bool %arr.load49, 1
  %elem.ptr51 = getelementptr i1, ptr %arr.data50, i64 %42
  %elem52 = load i1, ptr %elem.ptr51, align 1
  br i1 %elem52, label %then53, label %else55

then53:                                           ; preds = %else
  %printf.ret54 = call i32 (ptr, ...) @printf(ptr @.fmt.5)
  br label %merge72

else55:                                           ; preds = %else
  %visible.load = load i1, ptr %visible, align 1
  br i1 %visible.load, label %then56, label %else69

then56:                                           ; preds = %else55
  %t.load = load i8, ptr %t, align 1
  %ieq57 = icmp eq i8 %t.load, 35
  br i1 %ieq57, label %then58, label %else60

then58:                                           ; preds = %then56
  %printf.ret59 = call i32 (ptr, ...) @printf(ptr @.fmt.6)
  br label %merge68

else60:                                           ; preds = %then56
  %t.load61 = load i8, ptr %t, align 1
  %ieq62 = icmp eq i8 %t.load61, 42
  br i1 %ieq62, label %then63, label %else65

then63:                                           ; preds = %else60
  %printf.ret64 = call i32 (ptr, ...) @printf(ptr @.fmt.7)
  br label %merge

else65:                                           ; preds = %else60
  %t.load66 = load i8, ptr %t, align 1
  %printf.ret67 = call i32 (ptr, ...) @printf(ptr @.fmt.8, i8 %t.load66)
  br label %merge

merge:                                            ; preds = %else65, %then63
  br label %merge68

merge68:                                          ; preds = %merge, %then58
  br label %merge71

else69:                                           ; preds = %else55
  %printf.ret70 = call i32 (ptr, ...) @printf(ptr @.fmt.9)
  br label %merge71

merge71:                                          ; preds = %else69, %merge68
  br label %merge72

merge72:                                          ; preds = %merge71, %then53
  br label %merge73

merge73:                                          ; preds = %merge72, %then
  %y.load74 = load i32, ptr %y, align 4
  %addtmp75 = add i32 %y.load74, 1
  store i32 %addtmp75, ptr %y, align 4
  br label %while.cond20

while.exit76:                                     ; preds = %while.cond20
  %printf.ret77 = call i32 (ptr, ...) @printf(ptr @.fmt.10)
  %x.load78 = load i32, ptr %x, align 4
  %addtmp79 = add i32 %x.load78, 1
  store i32 %addtmp79, ptr %x, align 4
  br label %while.cond16

while.exit80:                                     ; preds = %while.cond16
  %43 = load %string, ptr %wall, align 8
  %44 = extractvalue %string %43, 0
  %printf.ret81 = call i32 (ptr, ...) @printf(ptr @.fmt.11, ptr %44)
  %printf.ret82 = call i32 (ptr, ...) @printf(ptr @.fmt.12)
  %45 = load %string, ptr %string, align 8
  %46 = extractvalue %string %45, 0
  %is.null85 = icmp eq ptr %46, null
  br i1 %is.null85, label %str.skip84, label %str.free83

str.free83:                                       ; preds = %while.exit80
  call void @free(ptr %46)
  br label %str.skip84

str.skip84:                                       ; preds = %str.free83, %while.exit80
  %47 = load %string, ptr %wall, align 8
  %48 = extractvalue %string %47, 0
  %is.null88 = icmp eq ptr %48, null
  br i1 %is.null88, label %str.skip87, label %str.free86

str.free86:                                       ; preds = %str.skip84
  call void @free(ptr %48)
  br label %str.skip87

str.skip87:                                       ; preds = %str.free86, %str.skip84
  ret void
}

define void @main() #0 {
entry:
  %time.val = call i64 @time(ptr null)
  %seed = trunc i64 %time.val to i32
  call void @srand(i32 %seed)
  %board = alloca %array.array.i8, align 8
  %0 = alloca %array.array.array.i8, align 8
  %1 = getelementptr inbounds nuw %array.array.array.i8, ptr %0, i32 0, i32 0
  store i64 10, ptr %1, align 4
  %2 = getelementptr inbounds nuw %array.array.array.i8, ptr %0, i32 0, i32 1
  %3 = call ptr @malloc(i64 160)
  store ptr %3, ptr %2, align 8
  %4 = alloca i64, align 8
  store i64 0, ptr %4, align 4
  br label %nd.loop

nd.loop:                                          ; preds = %nd.body, %entry
  %5 = load i64, ptr %4, align 4
  %6 = icmp ult i64 %5, 10
  br i1 %6, label %nd.body, label %nd.after

nd.body:                                          ; preds = %nd.loop
  %7 = getelementptr %array.array.array.i8, ptr %3, i64 %5
  %8 = alloca %array.array.array.i8, align 8
  %9 = getelementptr inbounds nuw %array.array.array.i8, ptr %8, i32 0, i32 0
  store i64 20, ptr %9, align 4
  %10 = getelementptr inbounds nuw %array.array.array.i8, ptr %8, i32 0, i32 1
  %11 = call ptr @malloc(i64 320)
  store ptr %11, ptr %10, align 8
  %12 = load %array.array.array.i8, ptr %8, align 8
  store %array.array.array.i8 %12, ptr %7, align 8
  %13 = add i64 %5, 1
  store i64 %13, ptr %4, align 4
  br label %nd.loop

nd.after:                                         ; preds = %nd.loop
  %board.arr.load = load %array.array.i8, ptr %0, align 8
  store %array.array.i8 %board.arr.load, ptr %board, align 8
  %mines = alloca %array.array.bool, align 8
  %14 = alloca %array.array.array.bool, align 8
  %15 = getelementptr inbounds nuw %array.array.array.bool, ptr %14, i32 0, i32 0
  store i64 10, ptr %15, align 4
  %16 = getelementptr inbounds nuw %array.array.array.bool, ptr %14, i32 0, i32 1
  %17 = call ptr @malloc(i64 160)
  store ptr %17, ptr %16, align 8
  %18 = alloca i64, align 8
  store i64 0, ptr %18, align 4
  br label %nd.loop1

nd.loop1:                                         ; preds = %nd.body2, %nd.after
  %19 = load i64, ptr %18, align 4
  %20 = icmp ult i64 %19, 10
  br i1 %20, label %nd.body2, label %nd.after3

nd.body2:                                         ; preds = %nd.loop1
  %21 = getelementptr %array.array.array.bool, ptr %17, i64 %19
  %22 = alloca %array.array.array.bool, align 8
  %23 = getelementptr inbounds nuw %array.array.array.bool, ptr %22, i32 0, i32 0
  store i64 20, ptr %23, align 4
  %24 = getelementptr inbounds nuw %array.array.array.bool, ptr %22, i32 0, i32 1
  %25 = call ptr @malloc(i64 320)
  store ptr %25, ptr %24, align 8
  %26 = load %array.array.array.bool, ptr %22, align 8
  store %array.array.array.bool %26, ptr %21, align 8
  %27 = add i64 %19, 1
  store i64 %27, ptr %18, align 4
  br label %nd.loop1

nd.after3:                                        ; preds = %nd.loop1
  %mines.arr.load = load %array.array.bool, ptr %14, align 8
  store %array.array.bool %mines.arr.load, ptr %mines, align 8
  %opened = alloca %array.array.bool, align 8
  %28 = alloca %array.array.array.bool, align 8
  %29 = getelementptr inbounds nuw %array.array.array.bool, ptr %28, i32 0, i32 0
  store i64 10, ptr %29, align 4
  %30 = getelementptr inbounds nuw %array.array.array.bool, ptr %28, i32 0, i32 1
  %31 = call ptr @malloc(i64 160)
  store ptr %31, ptr %30, align 8
  %32 = alloca i64, align 8
  store i64 0, ptr %32, align 4
  br label %nd.loop4

nd.loop4:                                         ; preds = %nd.body5, %nd.after3
  %33 = load i64, ptr %32, align 4
  %34 = icmp ult i64 %33, 10
  br i1 %34, label %nd.body5, label %nd.after6

nd.body5:                                         ; preds = %nd.loop4
  %35 = getelementptr %array.array.array.bool, ptr %31, i64 %33
  %36 = alloca %array.array.array.bool, align 8
  %37 = getelementptr inbounds nuw %array.array.array.bool, ptr %36, i32 0, i32 0
  store i64 20, ptr %37, align 4
  %38 = getelementptr inbounds nuw %array.array.array.bool, ptr %36, i32 0, i32 1
  %39 = call ptr @malloc(i64 320)
  store ptr %39, ptr %38, align 8
  %40 = load %array.array.array.bool, ptr %36, align 8
  store %array.array.array.bool %40, ptr %35, align 8
  %41 = add i64 %33, 1
  store i64 %41, ptr %32, align 4
  br label %nd.loop4

nd.after6:                                        ; preds = %nd.loop4
  %opened.arr.load = load %array.array.bool, ptr %28, align 8
  store %array.array.bool %opened.arr.load, ptr %opened, align 8
  %flags = alloca %array.array.bool, align 8
  %42 = alloca %array.array.array.bool, align 8
  %43 = getelementptr inbounds nuw %array.array.array.bool, ptr %42, i32 0, i32 0
  store i64 10, ptr %43, align 4
  %44 = getelementptr inbounds nuw %array.array.array.bool, ptr %42, i32 0, i32 1
  %45 = call ptr @malloc(i64 160)
  store ptr %45, ptr %44, align 8
  %46 = alloca i64, align 8
  store i64 0, ptr %46, align 4
  br label %nd.loop7

nd.loop7:                                         ; preds = %nd.body8, %nd.after6
  %47 = load i64, ptr %46, align 4
  %48 = icmp ult i64 %47, 10
  br i1 %48, label %nd.body8, label %nd.after9

nd.body8:                                         ; preds = %nd.loop7
  %49 = getelementptr %array.array.array.bool, ptr %45, i64 %47
  %50 = alloca %array.array.array.bool, align 8
  %51 = getelementptr inbounds nuw %array.array.array.bool, ptr %50, i32 0, i32 0
  store i64 20, ptr %51, align 4
  %52 = getelementptr inbounds nuw %array.array.array.bool, ptr %50, i32 0, i32 1
  %53 = call ptr @malloc(i64 320)
  store ptr %53, ptr %52, align 8
  %54 = load %array.array.array.bool, ptr %50, align 8
  store %array.array.array.bool %54, ptr %49, align 8
  %55 = add i64 %47, 1
  store i64 %55, ptr %46, align 4
  br label %nd.loop7

nd.after9:                                        ; preds = %nd.loop7
  %flags.arr.load = load %array.array.bool, ptr %42, align 8
  store %array.array.bool %flags.arr.load, ptr %flags, align 8
  call void @Generate(ptr %board, ptr %mines, ptr %opened, ptr %flags)
  call void @PlaceMines(ptr %mines)
  %cx = alloca i32, align 4
  store i32 0, ptr %cx, align 4
  %cy = alloca i32, align 4
  store i32 0, ptr %cy, align 4
  %found = alloca i1, align 1
  store i1 false, ptr %found, align 1
  br label %while.cond

while.cond:                                       ; preds = %merge, %nd.after9
  %found.load = load i1, ptr %found, align 1
  %lnot = xor i1 %found.load, true
  br i1 %lnot, label %while.body, label %while.exit

while.body:                                       ; preds = %while.cond
  %rand.val = call i32 @rand()
  %rand.f = sitofp i32 %rand.val to double
  %random.f = fdiv double %rand.f, 0x41DFFFFFFFC00000
  %fmul = fmul double %random.f, 1.000000e+01
  %fptosi = fptosi double %fmul to i32
  store i32 %fptosi, ptr %cx, align 4
  %rand.val10 = call i32 @rand()
  %rand.f11 = sitofp i32 %rand.val10 to double
  %random.f12 = fdiv double %rand.f11, 0x41DFFFFFFFC00000
  %fmul13 = fmul double %random.f12, 2.000000e+01
  %fptosi14 = fptosi double %fmul13 to i32
  store i32 %fptosi14, ptr %cy, align 4
  %nb = alloca i32, align 4
  %cx.load = load i32, ptr %cx, align 4
  %cy.load = load i32, ptr %cy, align 4
  %call = call i32 @Count(ptr %mines, i32 %cx.load, i32 %cy.load)
  store i32 %call, ptr %nb, align 4
  %cx.load15 = load i32, ptr %cx, align 4
  %56 = sext i32 %cx.load15 to i64
  %cy.load16 = load i32, ptr %cy, align 4
  %57 = sext i32 %cy.load16 to i64
  %nb.load = load i32, ptr %nb, align 4
  %58 = sext i32 %nb.load to i64
  %printf.ret = call i32 (ptr, ...) @printf(ptr @.fmt.13, i64 %56, i64 %57, i64 %58)
  %nb.load17 = load i32, ptr %nb, align 4
  %ieq = icmp eq i32 %nb.load17, 0
  %cx.load18 = load i32, ptr %cx, align 4
  %59 = sext i32 %cx.load18 to i64
  %arr.load = load %array.array.bool, ptr %mines, align 8
  %arr.data = extractvalue %array.array.bool %arr.load, 1
  %elem.ptr = getelementptr %array.bool, ptr %arr.data, i64 %59
  %cy.load19 = load i32, ptr %cy, align 4
  %60 = sext i32 %cy.load19 to i64
  %arr.load20 = load %array.bool, ptr %elem.ptr, align 8
  %arr.data21 = extractvalue %array.bool %arr.load20, 1
  %elem.ptr22 = getelementptr i1, ptr %arr.data21, i64 %60
  %elem = load i1, ptr %elem.ptr22, align 1
  %lnot23 = xor i1 %elem, true
  %l.bool = icmp ne i1 %ieq, false
  %r.bool = icmp ne i1 %lnot23, false
  %land = and i1 %l.bool, %r.bool
  br i1 %land, label %then, label %else

then:                                             ; preds = %while.body
  store i1 true, ptr %found, align 1
  br label %merge

else:                                             ; preds = %while.body
  br label %merge

merge:                                            ; preds = %else, %then
  br label %while.cond

while.exit:                                       ; preds = %while.cond
  %running = alloca i1, align 1
  store i1 true, ptr %running, align 1
  %gameOver = alloca i1, align 1
  store i1 false, ptr %gameOver, align 1
  br label %while.cond24

while.cond24:                                     ; preds = %merge326, %while.exit
  %running.load = load i1, ptr %running, align 1
  br i1 %running.load, label %while.body25, label %while.exit327

while.body25:                                     ; preds = %while.cond24
  %cx.load26 = load i32, ptr %cx, align 4
  %cy.load27 = load i32, ptr %cy, align 4
  call void @Draw(ptr %board, ptr %opened, ptr %flags, i32 %cx.load26, i32 %cy.load27)
  %gameOver.load = load i1, ptr %gameOver, align 1
  br i1 %gameOver.load, label %then28, label %else59

then28:                                           ; preds = %while.body25
  %printf.ret29 = call i32 (ptr, ...) @printf(ptr @.fmt.14)
  %61 = load %array.array.bool, ptr %flags, align 8
  %data = extractvalue %array.array.bool %61, 1
  %len = extractvalue %array.array.bool %61, 0
  %fi0 = alloca i64, align 8
  store i64 0, ptr %fi0, align 4
  br label %free.loop0

free.loop0:                                       ; preds = %free.body0, %then28
  %62 = load i64, ptr %fi0, align 4
  %63 = icmp ult i64 %62, %len
  br i1 %63, label %free.body0, label %free.after0

free.body0:                                       ; preds = %free.loop0
  %slot0 = getelementptr %array.bool, ptr %data, i64 %62
  %64 = load %array.bool, ptr %slot0, align 8
  %data30 = extractvalue %array.bool %64, 1
  %len31 = extractvalue %array.bool %64, 0
  call void @free(ptr %data30)
  %65 = add i64 %62, 1
  store i64 %65, ptr %fi0, align 4
  br label %free.loop0

free.after0:                                      ; preds = %free.loop0
  call void @free(ptr %data)
  %66 = load %array.array.bool, ptr %opened, align 8
  %data32 = extractvalue %array.array.bool %66, 1
  %len33 = extractvalue %array.array.bool %66, 0
  %fi037 = alloca i64, align 8
  store i64 0, ptr %fi037, align 4
  br label %free.loop034

free.loop034:                                     ; preds = %free.body035, %free.after0
  %67 = load i64, ptr %fi037, align 4
  %68 = icmp ult i64 %67, %len33
  br i1 %68, label %free.body035, label %free.after036

free.body035:                                     ; preds = %free.loop034
  %slot038 = getelementptr %array.bool, ptr %data32, i64 %67
  %69 = load %array.bool, ptr %slot038, align 8
  %data39 = extractvalue %array.bool %69, 1
  %len40 = extractvalue %array.bool %69, 0
  call void @free(ptr %data39)
  %70 = add i64 %67, 1
  store i64 %70, ptr %fi037, align 4
  br label %free.loop034

free.after036:                                    ; preds = %free.loop034
  call void @free(ptr %data32)
  %71 = load %array.array.bool, ptr %mines, align 8
  %data41 = extractvalue %array.array.bool %71, 1
  %len42 = extractvalue %array.array.bool %71, 0
  %fi046 = alloca i64, align 8
  store i64 0, ptr %fi046, align 4
  br label %free.loop043

free.loop043:                                     ; preds = %free.body044, %free.after036
  %72 = load i64, ptr %fi046, align 4
  %73 = icmp ult i64 %72, %len42
  br i1 %73, label %free.body044, label %free.after045

free.body044:                                     ; preds = %free.loop043
  %slot047 = getelementptr %array.bool, ptr %data41, i64 %72
  %74 = load %array.bool, ptr %slot047, align 8
  %data48 = extractvalue %array.bool %74, 1
  %len49 = extractvalue %array.bool %74, 0
  call void @free(ptr %data48)
  %75 = add i64 %72, 1
  store i64 %75, ptr %fi046, align 4
  br label %free.loop043

free.after045:                                    ; preds = %free.loop043
  call void @free(ptr %data41)
  %76 = load %array.array.i8, ptr %board, align 8
  %data50 = extractvalue %array.array.i8 %76, 1
  %len51 = extractvalue %array.array.i8 %76, 0
  %fi055 = alloca i64, align 8
  store i64 0, ptr %fi055, align 4
  br label %free.loop052

free.loop052:                                     ; preds = %free.body053, %free.after045
  %77 = load i64, ptr %fi055, align 4
  %78 = icmp ult i64 %77, %len51
  br i1 %78, label %free.body053, label %free.after054

free.body053:                                     ; preds = %free.loop052
  %slot056 = getelementptr %array.i8, ptr %data50, i64 %77
  %79 = load %array.i8, ptr %slot056, align 8
  %data57 = extractvalue %array.i8 %79, 1
  %len58 = extractvalue %array.i8 %79, 0
  call void @free(ptr %data57)
  %80 = add i64 %77, 1
  store i64 %80, ptr %fi055, align 4
  br label %free.loop052

free.after054:                                    ; preds = %free.loop052
  call void @free(ptr %data50)
  ret void

else59:                                           ; preds = %while.body25
  %call60 = call i1 @HasWon(ptr %mines, ptr %opened)
  br i1 %call60, label %then61, label %else99

then61:                                           ; preds = %else59
  %printf.ret62 = call i32 (ptr, ...) @printf(ptr @.fmt.15)
  %81 = load %array.array.bool, ptr %flags, align 8
  %data63 = extractvalue %array.array.bool %81, 1
  %len64 = extractvalue %array.array.bool %81, 0
  %fi068 = alloca i64, align 8
  store i64 0, ptr %fi068, align 4
  br label %free.loop065

free.loop065:                                     ; preds = %free.body066, %then61
  %82 = load i64, ptr %fi068, align 4
  %83 = icmp ult i64 %82, %len64
  br i1 %83, label %free.body066, label %free.after067

free.body066:                                     ; preds = %free.loop065
  %slot069 = getelementptr %array.bool, ptr %data63, i64 %82
  %84 = load %array.bool, ptr %slot069, align 8
  %data70 = extractvalue %array.bool %84, 1
  %len71 = extractvalue %array.bool %84, 0
  call void @free(ptr %data70)
  %85 = add i64 %82, 1
  store i64 %85, ptr %fi068, align 4
  br label %free.loop065

free.after067:                                    ; preds = %free.loop065
  call void @free(ptr %data63)
  %86 = load %array.array.bool, ptr %opened, align 8
  %data72 = extractvalue %array.array.bool %86, 1
  %len73 = extractvalue %array.array.bool %86, 0
  %fi077 = alloca i64, align 8
  store i64 0, ptr %fi077, align 4
  br label %free.loop074

free.loop074:                                     ; preds = %free.body075, %free.after067
  %87 = load i64, ptr %fi077, align 4
  %88 = icmp ult i64 %87, %len73
  br i1 %88, label %free.body075, label %free.after076

free.body075:                                     ; preds = %free.loop074
  %slot078 = getelementptr %array.bool, ptr %data72, i64 %87
  %89 = load %array.bool, ptr %slot078, align 8
  %data79 = extractvalue %array.bool %89, 1
  %len80 = extractvalue %array.bool %89, 0
  call void @free(ptr %data79)
  %90 = add i64 %87, 1
  store i64 %90, ptr %fi077, align 4
  br label %free.loop074

free.after076:                                    ; preds = %free.loop074
  call void @free(ptr %data72)
  %91 = load %array.array.bool, ptr %mines, align 8
  %data81 = extractvalue %array.array.bool %91, 1
  %len82 = extractvalue %array.array.bool %91, 0
  %fi086 = alloca i64, align 8
  store i64 0, ptr %fi086, align 4
  br label %free.loop083

free.loop083:                                     ; preds = %free.body084, %free.after076
  %92 = load i64, ptr %fi086, align 4
  %93 = icmp ult i64 %92, %len82
  br i1 %93, label %free.body084, label %free.after085

free.body084:                                     ; preds = %free.loop083
  %slot087 = getelementptr %array.bool, ptr %data81, i64 %92
  %94 = load %array.bool, ptr %slot087, align 8
  %data88 = extractvalue %array.bool %94, 1
  %len89 = extractvalue %array.bool %94, 0
  call void @free(ptr %data88)
  %95 = add i64 %92, 1
  store i64 %95, ptr %fi086, align 4
  br label %free.loop083

free.after085:                                    ; preds = %free.loop083
  call void @free(ptr %data81)
  %96 = load %array.array.i8, ptr %board, align 8
  %data90 = extractvalue %array.array.i8 %96, 1
  %len91 = extractvalue %array.array.i8 %96, 0
  %fi095 = alloca i64, align 8
  store i64 0, ptr %fi095, align 4
  br label %free.loop092

free.loop092:                                     ; preds = %free.body093, %free.after085
  %97 = load i64, ptr %fi095, align 4
  %98 = icmp ult i64 %97, %len91
  br i1 %98, label %free.body093, label %free.after094

free.body093:                                     ; preds = %free.loop092
  %slot096 = getelementptr %array.i8, ptr %data90, i64 %97
  %99 = load %array.i8, ptr %slot096, align 8
  %data97 = extractvalue %array.i8 %99, 1
  %len98 = extractvalue %array.i8 %99, 0
  call void @free(ptr %data97)
  %100 = add i64 %97, 1
  store i64 %100, ptr %fi095, align 4
  br label %free.loop092

free.after094:                                    ; preds = %free.loop092
  call void @free(ptr %data90)
  ret void

else99:                                           ; preds = %else59
  %input = alloca %string, align 8
  %read.buf = alloca i8, i32 1024, align 1
  %stdin.val = load ptr, ptr @stdin, align 8
  %101 = call ptr @fgets(ptr %read.buf, i32 1024, ptr %stdin.val)
  %read.len = call i64 @strlen(ptr %read.buf)
  %last.idx = sub i64 %read.len, 1
  %last.ptr = getelementptr i8, ptr %read.buf, i64 %last.idx
  %last.ch = load i8, ptr %last.ptr, align 1
  %102 = icmp eq i8 %last.ch, 10
  %103 = select i1 %102, i8 0, i8 %last.ch
  store i8 %103, ptr %last.ptr, align 1
  %104 = sub i64 %read.len, 1
  %trimmed.len = select i1 %102, i64 %104, i64 %read.len
  %cap = add i64 %trimmed.len, 1
  %str.alloc = call ptr @malloc(i64 %cap)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc, ptr %read.buf, i64 %trimmed.len, i1 false)
  %105 = getelementptr i8, ptr %str.alloc, i64 %trimmed.len
  store i8 0, ptr %105, align 1
  %string = alloca %string, align 8
  %106 = getelementptr inbounds nuw %string, ptr %string, i32 0, i32 0
  store ptr %str.alloc, ptr %106, align 8
  %107 = getelementptr inbounds nuw %string, ptr %string, i32 0, i32 1
  store i64 %trimmed.len, ptr %107, align 4
  %108 = getelementptr inbounds nuw %string, ptr %string, i32 0, i32 2
  store i64 %cap, ptr %108, align 4
  %str.alloc100 = call ptr @malloc(i64 1)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc100, ptr @strl.16, i64 0, i1 false)
  %109 = getelementptr i8, ptr %str.alloc100, i64 0
  store i8 0, ptr %109, align 1
  %string101 = alloca %string, align 8
  %110 = getelementptr inbounds nuw %string, ptr %string101, i32 0, i32 0
  store ptr %str.alloc100, ptr %110, align 8
  %111 = getelementptr inbounds nuw %string, ptr %string101, i32 0, i32 1
  store i64 0, ptr %111, align 4
  %112 = getelementptr inbounds nuw %string, ptr %string101, i32 0, i32 2
  store i64 1, ptr %112, align 4
  %ls = load %string, ptr %string, align 8
  %rs = load %string, ptr %string101, align 8
  %ld = extractvalue %string %ls, 0
  %ll = extractvalue %string %ls, 1
  %rd = extractvalue %string %rs, 0
  %rl = extractvalue %string %rs, 1
  %tlen = add i64 %ll, %rl
  %tcap = add i64 %tlen, 1
  %cat.alloc = call ptr @malloc(i64 %tcap)
  call void @llvm.memcpy.p0.p0.i64(ptr %cat.alloc, ptr %ld, i64 %ll, i1 false)
  %113 = getelementptr i8, ptr %cat.alloc, i64 %ll
  call void @llvm.memcpy.p0.p0.i64(ptr %113, ptr %rd, i64 %rl, i1 false)
  %114 = getelementptr i8, ptr %cat.alloc, i64 %tlen
  store i8 0, ptr %114, align 1
  %cat.res = alloca %string, align 8
  %115 = getelementptr inbounds nuw %string, ptr %cat.res, i32 0, i32 0
  store ptr %cat.alloc, ptr %115, align 8
  %116 = getelementptr inbounds nuw %string, ptr %cat.res, i32 0, i32 1
  store i64 %tlen, ptr %116, align 4
  %117 = getelementptr inbounds nuw %string, ptr %cat.res, i32 0, i32 2
  store i64 %tcap, ptr %117, align 4
  %118 = load %string, ptr %cat.res, align 8
  store %string %118, ptr %input, align 8
  %119 = load %string, ptr %string101, align 8
  %empty.data = extractvalue %string %119, 0
  call void @free(ptr %empty.data)
  %120 = load %string, ptr %string, align 8
  %init.data = extractvalue %string %120, 0
  call void @free(ptr %init.data)
  %str.alloc102 = call ptr @malloc(i64 2)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc102, ptr @strl.17, i64 1, i1 false)
  %121 = getelementptr i8, ptr %str.alloc102, i64 1
  store i8 0, ptr %121, align 1
  %string103 = alloca %string, align 8
  %122 = getelementptr inbounds nuw %string, ptr %string103, i32 0, i32 0
  store ptr %str.alloc102, ptr %122, align 8
  %123 = getelementptr inbounds nuw %string, ptr %string103, i32 0, i32 1
  store i64 1, ptr %123, align 4
  %124 = getelementptr inbounds nuw %string, ptr %string103, i32 0, i32 2
  store i64 2, ptr %124, align 4
  %eq.ls = load %string, ptr %input, align 8
  %eq.rs = load %string, ptr %string103, align 8
  %eq.ld = extractvalue %string %eq.ls, 0
  %eq.rd = extractvalue %string %eq.rs, 0
  %strcmp = call i32 @strcmp(ptr %eq.ld, ptr %eq.rd)
  %streq = icmp eq i32 %strcmp, 0
  br i1 %streq, label %then104, label %else110

then104:                                          ; preds = %else99
  %cx.load105 = load i32, ptr %cx, align 4
  %igt = icmp sgt i32 %cx.load105, 0
  br i1 %igt, label %then106, label %else108

then106:                                          ; preds = %then104
  %cx.load107 = load i32, ptr %cx, align 4
  %sub = sub i32 %cx.load107, 1
  store i32 %sub, ptr %cx, align 4
  br label %merge109

else108:                                          ; preds = %then104
  br label %merge109

merge109:                                         ; preds = %else108, %then106
  br label %merge111

else110:                                          ; preds = %else99
  br label %merge111

merge111:                                         ; preds = %else110, %merge109
  %str.alloc112 = call ptr @malloc(i64 2)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc112, ptr @strl.18, i64 1, i1 false)
  %125 = getelementptr i8, ptr %str.alloc112, i64 1
  store i8 0, ptr %125, align 1
  %string113 = alloca %string, align 8
  %126 = getelementptr inbounds nuw %string, ptr %string113, i32 0, i32 0
  store ptr %str.alloc112, ptr %126, align 8
  %127 = getelementptr inbounds nuw %string, ptr %string113, i32 0, i32 1
  store i64 1, ptr %127, align 4
  %128 = getelementptr inbounds nuw %string, ptr %string113, i32 0, i32 2
  store i64 2, ptr %128, align 4
  %eq.ls114 = load %string, ptr %input, align 8
  %eq.rs115 = load %string, ptr %string113, align 8
  %eq.ld116 = extractvalue %string %eq.ls114, 0
  %eq.rd117 = extractvalue %string %eq.rs115, 0
  %strcmp118 = call i32 @strcmp(ptr %eq.ld116, ptr %eq.rd117)
  %streq119 = icmp eq i32 %strcmp118, 0
  br i1 %streq119, label %then120, label %else126

then120:                                          ; preds = %merge111
  %cx.load121 = load i32, ptr %cx, align 4
  %ilt = icmp slt i32 %cx.load121, 9
  br i1 %ilt, label %then122, label %else124

then122:                                          ; preds = %then120
  %cx.load123 = load i32, ptr %cx, align 4
  %addtmp = add i32 %cx.load123, 1
  store i32 %addtmp, ptr %cx, align 4
  br label %merge125

else124:                                          ; preds = %then120
  br label %merge125

merge125:                                         ; preds = %else124, %then122
  br label %merge127

else126:                                          ; preds = %merge111
  br label %merge127

merge127:                                         ; preds = %else126, %merge125
  %str.alloc128 = call ptr @malloc(i64 2)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc128, ptr @strl.19, i64 1, i1 false)
  %129 = getelementptr i8, ptr %str.alloc128, i64 1
  store i8 0, ptr %129, align 1
  %string129 = alloca %string, align 8
  %130 = getelementptr inbounds nuw %string, ptr %string129, i32 0, i32 0
  store ptr %str.alloc128, ptr %130, align 8
  %131 = getelementptr inbounds nuw %string, ptr %string129, i32 0, i32 1
  store i64 1, ptr %131, align 4
  %132 = getelementptr inbounds nuw %string, ptr %string129, i32 0, i32 2
  store i64 2, ptr %132, align 4
  %eq.ls130 = load %string, ptr %input, align 8
  %eq.rs131 = load %string, ptr %string129, align 8
  %eq.ld132 = extractvalue %string %eq.ls130, 0
  %eq.rd133 = extractvalue %string %eq.rs131, 0
  %strcmp134 = call i32 @strcmp(ptr %eq.ld132, ptr %eq.rd133)
  %streq135 = icmp eq i32 %strcmp134, 0
  br i1 %streq135, label %then136, label %else144

then136:                                          ; preds = %merge127
  %cy.load137 = load i32, ptr %cy, align 4
  %igt138 = icmp sgt i32 %cy.load137, 0
  br i1 %igt138, label %then139, label %else142

then139:                                          ; preds = %then136
  %cy.load140 = load i32, ptr %cy, align 4
  %sub141 = sub i32 %cy.load140, 1
  store i32 %sub141, ptr %cy, align 4
  br label %merge143

else142:                                          ; preds = %then136
  br label %merge143

merge143:                                         ; preds = %else142, %then139
  br label %merge145

else144:                                          ; preds = %merge127
  br label %merge145

merge145:                                         ; preds = %else144, %merge143
  %str.alloc146 = call ptr @malloc(i64 2)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc146, ptr @strl.20, i64 1, i1 false)
  %133 = getelementptr i8, ptr %str.alloc146, i64 1
  store i8 0, ptr %133, align 1
  %string147 = alloca %string, align 8
  %134 = getelementptr inbounds nuw %string, ptr %string147, i32 0, i32 0
  store ptr %str.alloc146, ptr %134, align 8
  %135 = getelementptr inbounds nuw %string, ptr %string147, i32 0, i32 1
  store i64 1, ptr %135, align 4
  %136 = getelementptr inbounds nuw %string, ptr %string147, i32 0, i32 2
  store i64 2, ptr %136, align 4
  %eq.ls148 = load %string, ptr %input, align 8
  %eq.rs149 = load %string, ptr %string147, align 8
  %eq.ld150 = extractvalue %string %eq.ls148, 0
  %eq.rd151 = extractvalue %string %eq.rs149, 0
  %strcmp152 = call i32 @strcmp(ptr %eq.ld150, ptr %eq.rd151)
  %streq153 = icmp eq i32 %strcmp152, 0
  br i1 %streq153, label %then154, label %else162

then154:                                          ; preds = %merge145
  %cy.load155 = load i32, ptr %cy, align 4
  %ilt156 = icmp slt i32 %cy.load155, 19
  br i1 %ilt156, label %then157, label %else160

then157:                                          ; preds = %then154
  %cy.load158 = load i32, ptr %cy, align 4
  %addtmp159 = add i32 %cy.load158, 1
  store i32 %addtmp159, ptr %cy, align 4
  br label %merge161

else160:                                          ; preds = %then154
  br label %merge161

merge161:                                         ; preds = %else160, %then157
  br label %merge163

else162:                                          ; preds = %merge145
  br label %merge163

merge163:                                         ; preds = %else162, %merge161
  %str.alloc164 = call ptr @malloc(i64 2)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc164, ptr @strl.21, i64 1, i1 false)
  %137 = getelementptr i8, ptr %str.alloc164, i64 1
  store i8 0, ptr %137, align 1
  %string165 = alloca %string, align 8
  %138 = getelementptr inbounds nuw %string, ptr %string165, i32 0, i32 0
  store ptr %str.alloc164, ptr %138, align 8
  %139 = getelementptr inbounds nuw %string, ptr %string165, i32 0, i32 1
  store i64 1, ptr %139, align 4
  %140 = getelementptr inbounds nuw %string, ptr %string165, i32 0, i32 2
  store i64 2, ptr %140, align 4
  %eq.ls166 = load %string, ptr %input, align 8
  %eq.rs167 = load %string, ptr %string165, align 8
  %eq.ld168 = extractvalue %string %eq.ls166, 0
  %eq.rd169 = extractvalue %string %eq.rs167, 0
  %strcmp170 = call i32 @strcmp(ptr %eq.ld168, ptr %eq.rd169)
  %streq171 = icmp eq i32 %strcmp170, 0
  br i1 %streq171, label %then172, label %else191

then172:                                          ; preds = %merge163
  %cx.load173 = load i32, ptr %cx, align 4
  %141 = sext i32 %cx.load173 to i64
  %arr.load174 = load %array.array.bool, ptr %flags, align 8
  %arr.data175 = extractvalue %array.array.bool %arr.load174, 1
  %elem.ptr176 = getelementptr %array.bool, ptr %arr.data175, i64 %141
  %cy.load177 = load i32, ptr %cy, align 4
  %142 = sext i32 %cy.load177 to i64
  %arr.load178 = load %array.bool, ptr %elem.ptr176, align 8
  %arr.data179 = extractvalue %array.bool %arr.load178, 1
  %elem.ptr180 = getelementptr i1, ptr %arr.data179, i64 %142
  %cx.load181 = load i32, ptr %cx, align 4
  %143 = sext i32 %cx.load181 to i64
  %arr.load182 = load %array.array.bool, ptr %flags, align 8
  %arr.data183 = extractvalue %array.array.bool %arr.load182, 1
  %elem.ptr184 = getelementptr %array.bool, ptr %arr.data183, i64 %143
  %cy.load185 = load i32, ptr %cy, align 4
  %144 = sext i32 %cy.load185 to i64
  %arr.load186 = load %array.bool, ptr %elem.ptr184, align 8
  %arr.data187 = extractvalue %array.bool %arr.load186, 1
  %elem.ptr188 = getelementptr i1, ptr %arr.data187, i64 %144
  %elem189 = load i1, ptr %elem.ptr188, align 1
  %lnot190 = xor i1 %elem189, true
  store i1 %lnot190, ptr %elem.ptr180, align 1
  br label %merge192

else191:                                          ; preds = %merge163
  br label %merge192

merge192:                                         ; preds = %else191, %then172
  %str.alloc193 = call ptr @malloc(i64 2)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc193, ptr @strl.22, i64 1, i1 false)
  %145 = getelementptr i8, ptr %str.alloc193, i64 1
  store i8 0, ptr %145, align 1
  %string194 = alloca %string, align 8
  %146 = getelementptr inbounds nuw %string, ptr %string194, i32 0, i32 0
  store ptr %str.alloc193, ptr %146, align 8
  %147 = getelementptr inbounds nuw %string, ptr %string194, i32 0, i32 1
  store i64 1, ptr %147, align 4
  %148 = getelementptr inbounds nuw %string, ptr %string194, i32 0, i32 2
  store i64 2, ptr %148, align 4
  %eq.ls195 = load %string, ptr %input, align 8
  %eq.rs196 = load %string, ptr %string194, align 8
  %eq.ld197 = extractvalue %string %eq.ls195, 0
  %eq.rd198 = extractvalue %string %eq.rs196, 0
  %strcmp199 = call i32 @strcmp(ptr %eq.ld197, ptr %eq.rd198)
  %streq200 = icmp eq i32 %strcmp199, 0
  br i1 %streq200, label %then201, label %else230

then201:                                          ; preds = %merge192
  %cx.load202 = load i32, ptr %cx, align 4
  %149 = sext i32 %cx.load202 to i64
  %arr.load203 = load %array.array.bool, ptr %opened, align 8
  %arr.data204 = extractvalue %array.array.bool %arr.load203, 1
  %elem.ptr205 = getelementptr %array.bool, ptr %arr.data204, i64 %149
  %cy.load206 = load i32, ptr %cy, align 4
  %150 = sext i32 %cy.load206 to i64
  %arr.load207 = load %array.bool, ptr %elem.ptr205, align 8
  %arr.data208 = extractvalue %array.bool %arr.load207, 1
  %elem.ptr209 = getelementptr i1, ptr %arr.data208, i64 %150
  %elem210 = load i1, ptr %elem.ptr209, align 1
  %lnot211 = xor i1 %elem210, true
  br i1 %lnot211, label %then212, label %else228

then212:                                          ; preds = %then201
  %printf.ret213 = call i32 (ptr, ...) @printf(ptr @.fmt.23)
  %cx.load214 = load i32, ptr %cx, align 4
  %cy.load215 = load i32, ptr %cy, align 4
  call void @Open(ptr %board, ptr %mines, ptr %opened, i32 %cx.load214, i32 %cy.load215)
  %cx.load216 = load i32, ptr %cx, align 4
  %151 = sext i32 %cx.load216 to i64
  %arr.load217 = load %array.array.bool, ptr %mines, align 8
  %arr.data218 = extractvalue %array.array.bool %arr.load217, 1
  %elem.ptr219 = getelementptr %array.bool, ptr %arr.data218, i64 %151
  %cy.load220 = load i32, ptr %cy, align 4
  %152 = sext i32 %cy.load220 to i64
  %arr.load221 = load %array.bool, ptr %elem.ptr219, align 8
  %arr.data222 = extractvalue %array.bool %arr.load221, 1
  %elem.ptr223 = getelementptr i1, ptr %arr.data222, i64 %152
  %elem224 = load i1, ptr %elem.ptr223, align 1
  br i1 %elem224, label %then225, label %else226

then225:                                          ; preds = %then212
  store i1 true, ptr %gameOver, align 1
  br label %merge227

else226:                                          ; preds = %then212
  br label %merge227

merge227:                                         ; preds = %else226, %then225
  br label %merge229

else228:                                          ; preds = %then201
  br label %merge229

merge229:                                         ; preds = %else228, %merge227
  br label %merge231

else230:                                          ; preds = %merge192
  br label %merge231

merge231:                                         ; preds = %else230, %merge229
  %str.alloc232 = call ptr @malloc(i64 2)
  call void @llvm.memcpy.p0.p0.i64(ptr %str.alloc232, ptr @strl.24, i64 1, i1 false)
  %153 = getelementptr i8, ptr %str.alloc232, i64 1
  store i8 0, ptr %153, align 1
  %string233 = alloca %string, align 8
  %154 = getelementptr inbounds nuw %string, ptr %string233, i32 0, i32 0
  store ptr %str.alloc232, ptr %154, align 8
  %155 = getelementptr inbounds nuw %string, ptr %string233, i32 0, i32 1
  store i64 1, ptr %155, align 4
  %156 = getelementptr inbounds nuw %string, ptr %string233, i32 0, i32 2
  store i64 2, ptr %156, align 4
  %eq.ls234 = load %string, ptr %input, align 8
  %eq.rs235 = load %string, ptr %string233, align 8
  %eq.ld236 = extractvalue %string %eq.ls234, 0
  %eq.rd237 = extractvalue %string %eq.rs235, 0
  %strcmp238 = call i32 @strcmp(ptr %eq.ld236, ptr %eq.rd237)
  %streq239 = icmp eq i32 %strcmp238, 0
  br i1 %streq239, label %then240, label %else299

then240:                                          ; preds = %merge231
  %printf.ret241 = call i32 (ptr, ...) @printf(ptr @.fmt.25)
  %157 = load %string, ptr %string103, align 8
  %158 = extractvalue %string %157, 0
  %is.null = icmp eq ptr %158, null
  br i1 %is.null, label %str.skip, label %str.free

str.free:                                         ; preds = %then240
  call void @free(ptr %158)
  br label %str.skip

str.skip:                                         ; preds = %str.free, %then240
  %159 = load %string, ptr %string113, align 8
  %160 = extractvalue %string %159, 0
  %is.null244 = icmp eq ptr %160, null
  br i1 %is.null244, label %str.skip243, label %str.free242

str.free242:                                      ; preds = %str.skip
  call void @free(ptr %160)
  br label %str.skip243

str.skip243:                                      ; preds = %str.free242, %str.skip
  %161 = load %string, ptr %string129, align 8
  %162 = extractvalue %string %161, 0
  %is.null247 = icmp eq ptr %162, null
  br i1 %is.null247, label %str.skip246, label %str.free245

str.free245:                                      ; preds = %str.skip243
  call void @free(ptr %162)
  br label %str.skip246

str.skip246:                                      ; preds = %str.free245, %str.skip243
  %163 = load %string, ptr %string147, align 8
  %164 = extractvalue %string %163, 0
  %is.null250 = icmp eq ptr %164, null
  br i1 %is.null250, label %str.skip249, label %str.free248

str.free248:                                      ; preds = %str.skip246
  call void @free(ptr %164)
  br label %str.skip249

str.skip249:                                      ; preds = %str.free248, %str.skip246
  %165 = load %string, ptr %string165, align 8
  %166 = extractvalue %string %165, 0
  %is.null253 = icmp eq ptr %166, null
  br i1 %is.null253, label %str.skip252, label %str.free251

str.free251:                                      ; preds = %str.skip249
  call void @free(ptr %166)
  br label %str.skip252

str.skip252:                                      ; preds = %str.free251, %str.skip249
  %167 = load %string, ptr %string194, align 8
  %168 = extractvalue %string %167, 0
  %is.null256 = icmp eq ptr %168, null
  br i1 %is.null256, label %str.skip255, label %str.free254

str.free254:                                      ; preds = %str.skip252
  call void @free(ptr %168)
  br label %str.skip255

str.skip255:                                      ; preds = %str.free254, %str.skip252
  %169 = load %string, ptr %string233, align 8
  %170 = extractvalue %string %169, 0
  %is.null259 = icmp eq ptr %170, null
  br i1 %is.null259, label %str.skip258, label %str.free257

str.free257:                                      ; preds = %str.skip255
  call void @free(ptr %170)
  br label %str.skip258

str.skip258:                                      ; preds = %str.free257, %str.skip255
  %171 = load %string, ptr %input, align 8
  %172 = extractvalue %string %171, 0
  %is.null262 = icmp eq ptr %172, null
  br i1 %is.null262, label %str.skip261, label %str.free260

str.free260:                                      ; preds = %str.skip258
  call void @free(ptr %172)
  br label %str.skip261

str.skip261:                                      ; preds = %str.free260, %str.skip258
  %173 = load %array.array.bool, ptr %flags, align 8
  %data263 = extractvalue %array.array.bool %173, 1
  %len264 = extractvalue %array.array.bool %173, 0
  %fi0268 = alloca i64, align 8
  store i64 0, ptr %fi0268, align 4
  br label %free.loop0265

free.loop0265:                                    ; preds = %free.body0266, %str.skip261
  %174 = load i64, ptr %fi0268, align 4
  %175 = icmp ult i64 %174, %len264
  br i1 %175, label %free.body0266, label %free.after0267

free.body0266:                                    ; preds = %free.loop0265
  %slot0269 = getelementptr %array.bool, ptr %data263, i64 %174
  %176 = load %array.bool, ptr %slot0269, align 8
  %data270 = extractvalue %array.bool %176, 1
  %len271 = extractvalue %array.bool %176, 0
  call void @free(ptr %data270)
  %177 = add i64 %174, 1
  store i64 %177, ptr %fi0268, align 4
  br label %free.loop0265

free.after0267:                                   ; preds = %free.loop0265
  call void @free(ptr %data263)
  %178 = load %array.array.bool, ptr %opened, align 8
  %data272 = extractvalue %array.array.bool %178, 1
  %len273 = extractvalue %array.array.bool %178, 0
  %fi0277 = alloca i64, align 8
  store i64 0, ptr %fi0277, align 4
  br label %free.loop0274

free.loop0274:                                    ; preds = %free.body0275, %free.after0267
  %179 = load i64, ptr %fi0277, align 4
  %180 = icmp ult i64 %179, %len273
  br i1 %180, label %free.body0275, label %free.after0276

free.body0275:                                    ; preds = %free.loop0274
  %slot0278 = getelementptr %array.bool, ptr %data272, i64 %179
  %181 = load %array.bool, ptr %slot0278, align 8
  %data279 = extractvalue %array.bool %181, 1
  %len280 = extractvalue %array.bool %181, 0
  call void @free(ptr %data279)
  %182 = add i64 %179, 1
  store i64 %182, ptr %fi0277, align 4
  br label %free.loop0274

free.after0276:                                   ; preds = %free.loop0274
  call void @free(ptr %data272)
  %183 = load %array.array.bool, ptr %mines, align 8
  %data281 = extractvalue %array.array.bool %183, 1
  %len282 = extractvalue %array.array.bool %183, 0
  %fi0286 = alloca i64, align 8
  store i64 0, ptr %fi0286, align 4
  br label %free.loop0283

free.loop0283:                                    ; preds = %free.body0284, %free.after0276
  %184 = load i64, ptr %fi0286, align 4
  %185 = icmp ult i64 %184, %len282
  br i1 %185, label %free.body0284, label %free.after0285

free.body0284:                                    ; preds = %free.loop0283
  %slot0287 = getelementptr %array.bool, ptr %data281, i64 %184
  %186 = load %array.bool, ptr %slot0287, align 8
  %data288 = extractvalue %array.bool %186, 1
  %len289 = extractvalue %array.bool %186, 0
  call void @free(ptr %data288)
  %187 = add i64 %184, 1
  store i64 %187, ptr %fi0286, align 4
  br label %free.loop0283

free.after0285:                                   ; preds = %free.loop0283
  call void @free(ptr %data281)
  %188 = load %array.array.i8, ptr %board, align 8
  %data290 = extractvalue %array.array.i8 %188, 1
  %len291 = extractvalue %array.array.i8 %188, 0
  %fi0295 = alloca i64, align 8
  store i64 0, ptr %fi0295, align 4
  br label %free.loop0292

free.loop0292:                                    ; preds = %free.body0293, %free.after0285
  %189 = load i64, ptr %fi0295, align 4
  %190 = icmp ult i64 %189, %len291
  br i1 %190, label %free.body0293, label %free.after0294

free.body0293:                                    ; preds = %free.loop0292
  %slot0296 = getelementptr %array.i8, ptr %data290, i64 %189
  %191 = load %array.i8, ptr %slot0296, align 8
  %data297 = extractvalue %array.i8 %191, 1
  %len298 = extractvalue %array.i8 %191, 0
  call void @free(ptr %data297)
  %192 = add i64 %189, 1
  store i64 %192, ptr %fi0295, align 4
  br label %free.loop0292

free.after0294:                                   ; preds = %free.loop0292
  call void @free(ptr %data290)
  ret void

else299:                                          ; preds = %merge231
  br label %merge300

merge300:                                         ; preds = %else299
  %193 = load %string, ptr %string103, align 8
  %194 = extractvalue %string %193, 0
  %is.null303 = icmp eq ptr %194, null
  br i1 %is.null303, label %str.skip302, label %str.free301

str.free301:                                      ; preds = %merge300
  call void @free(ptr %194)
  br label %str.skip302

str.skip302:                                      ; preds = %str.free301, %merge300
  %195 = load %string, ptr %string113, align 8
  %196 = extractvalue %string %195, 0
  %is.null306 = icmp eq ptr %196, null
  br i1 %is.null306, label %str.skip305, label %str.free304

str.free304:                                      ; preds = %str.skip302
  call void @free(ptr %196)
  br label %str.skip305

str.skip305:                                      ; preds = %str.free304, %str.skip302
  %197 = load %string, ptr %string129, align 8
  %198 = extractvalue %string %197, 0
  %is.null309 = icmp eq ptr %198, null
  br i1 %is.null309, label %str.skip308, label %str.free307

str.free307:                                      ; preds = %str.skip305
  call void @free(ptr %198)
  br label %str.skip308

str.skip308:                                      ; preds = %str.free307, %str.skip305
  %199 = load %string, ptr %string147, align 8
  %200 = extractvalue %string %199, 0
  %is.null312 = icmp eq ptr %200, null
  br i1 %is.null312, label %str.skip311, label %str.free310

str.free310:                                      ; preds = %str.skip308
  call void @free(ptr %200)
  br label %str.skip311

str.skip311:                                      ; preds = %str.free310, %str.skip308
  %201 = load %string, ptr %string165, align 8
  %202 = extractvalue %string %201, 0
  %is.null315 = icmp eq ptr %202, null
  br i1 %is.null315, label %str.skip314, label %str.free313

str.free313:                                      ; preds = %str.skip311
  call void @free(ptr %202)
  br label %str.skip314

str.skip314:                                      ; preds = %str.free313, %str.skip311
  %203 = load %string, ptr %string194, align 8
  %204 = extractvalue %string %203, 0
  %is.null318 = icmp eq ptr %204, null
  br i1 %is.null318, label %str.skip317, label %str.free316

str.free316:                                      ; preds = %str.skip314
  call void @free(ptr %204)
  br label %str.skip317

str.skip317:                                      ; preds = %str.free316, %str.skip314
  %205 = load %string, ptr %string233, align 8
  %206 = extractvalue %string %205, 0
  %is.null321 = icmp eq ptr %206, null
  br i1 %is.null321, label %str.skip320, label %str.free319

str.free319:                                      ; preds = %str.skip317
  call void @free(ptr %206)
  br label %str.skip320

str.skip320:                                      ; preds = %str.free319, %str.skip317
  %207 = load %string, ptr %input, align 8
  %208 = extractvalue %string %207, 0
  %is.null324 = icmp eq ptr %208, null
  br i1 %is.null324, label %str.skip323, label %str.free322

str.free322:                                      ; preds = %str.skip320
  call void @free(ptr %208)
  br label %str.skip323

str.skip323:                                      ; preds = %str.free322, %str.skip320
  br label %merge325

merge325:                                         ; preds = %str.skip323
  br label %merge326

merge326:                                         ; preds = %merge325
  br label %while.cond24

while.exit327:                                    ; preds = %while.cond24
  %209 = load %array.array.bool, ptr %flags, align 8
  %data328 = extractvalue %array.array.bool %209, 1
  %len329 = extractvalue %array.array.bool %209, 0
  %fi0333 = alloca i64, align 8
  store i64 0, ptr %fi0333, align 4
  br label %free.loop0330

free.loop0330:                                    ; preds = %free.body0331, %while.exit327
  %210 = load i64, ptr %fi0333, align 4
  %211 = icmp ult i64 %210, %len329
  br i1 %211, label %free.body0331, label %free.after0332

free.body0331:                                    ; preds = %free.loop0330
  %slot0334 = getelementptr %array.bool, ptr %data328, i64 %210
  %212 = load %array.bool, ptr %slot0334, align 8
  %data335 = extractvalue %array.bool %212, 1
  %len336 = extractvalue %array.bool %212, 0
  call void @free(ptr %data335)
  %213 = add i64 %210, 1
  store i64 %213, ptr %fi0333, align 4
  br label %free.loop0330

free.after0332:                                   ; preds = %free.loop0330
  call void @free(ptr %data328)
  %214 = load %array.array.bool, ptr %opened, align 8
  %data337 = extractvalue %array.array.bool %214, 1
  %len338 = extractvalue %array.array.bool %214, 0
  %fi0342 = alloca i64, align 8
  store i64 0, ptr %fi0342, align 4
  br label %free.loop0339

free.loop0339:                                    ; preds = %free.body0340, %free.after0332
  %215 = load i64, ptr %fi0342, align 4
  %216 = icmp ult i64 %215, %len338
  br i1 %216, label %free.body0340, label %free.after0341

free.body0340:                                    ; preds = %free.loop0339
  %slot0343 = getelementptr %array.bool, ptr %data337, i64 %215
  %217 = load %array.bool, ptr %slot0343, align 8
  %data344 = extractvalue %array.bool %217, 1
  %len345 = extractvalue %array.bool %217, 0
  call void @free(ptr %data344)
  %218 = add i64 %215, 1
  store i64 %218, ptr %fi0342, align 4
  br label %free.loop0339

free.after0341:                                   ; preds = %free.loop0339
  call void @free(ptr %data337)
  %219 = load %array.array.bool, ptr %mines, align 8
  %data346 = extractvalue %array.array.bool %219, 1
  %len347 = extractvalue %array.array.bool %219, 0
  %fi0351 = alloca i64, align 8
  store i64 0, ptr %fi0351, align 4
  br label %free.loop0348

free.loop0348:                                    ; preds = %free.body0349, %free.after0341
  %220 = load i64, ptr %fi0351, align 4
  %221 = icmp ult i64 %220, %len347
  br i1 %221, label %free.body0349, label %free.after0350

free.body0349:                                    ; preds = %free.loop0348
  %slot0352 = getelementptr %array.bool, ptr %data346, i64 %220
  %222 = load %array.bool, ptr %slot0352, align 8
  %data353 = extractvalue %array.bool %222, 1
  %len354 = extractvalue %array.bool %222, 0
  call void @free(ptr %data353)
  %223 = add i64 %220, 1
  store i64 %223, ptr %fi0351, align 4
  br label %free.loop0348

free.after0350:                                   ; preds = %free.loop0348
  call void @free(ptr %data346)
  %224 = load %array.array.i8, ptr %board, align 8
  %data355 = extractvalue %array.array.i8 %224, 1
  %len356 = extractvalue %array.array.i8 %224, 0
  %fi0360 = alloca i64, align 8
  store i64 0, ptr %fi0360, align 4
  br label %free.loop0357

free.loop0357:                                    ; preds = %free.body0358, %free.after0350
  %225 = load i64, ptr %fi0360, align 4
  %226 = icmp ult i64 %225, %len356
  br i1 %226, label %free.body0358, label %free.after0359

free.body0358:                                    ; preds = %free.loop0357
  %slot0361 = getelementptr %array.i8, ptr %data355, i64 %225
  %227 = load %array.i8, ptr %slot0361, align 8
  %data362 = extractvalue %array.i8 %227, 1
  %len363 = extractvalue %array.i8 %227, 0
  call void @free(ptr %data362)
  %228 = add i64 %225, 1
  store i64 %228, ptr %fi0360, align 4
  br label %free.loop0357

free.after0359:                                   ; preds = %free.loop0357
  call void @free(ptr %data355)
  ret void
}

declare i32 @rand()

declare ptr @malloc(i64)

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly captures(none), ptr noalias readonly captures(none), i64, i1 immarg) #1

declare void @free(ptr)

declare i64 @time(ptr)

declare void @srand(i32)

declare ptr @fgets(ptr, i32, ptr)

declare i64 @strlen(ptr)

attributes #0 = { "stackrealignment" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
