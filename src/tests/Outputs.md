** Here you'll find the outputs of the tests **

The executed tests are outputs of the examples in the `examples` folder when ran with the ast tool. Please do not run more tests, notify the user when you done with the fixes. Your job is to crossreference the specs. the outputs and the code on the examples to determine correctness of the lexer and parser. Fix whatever error you encounter.

## basic.runes

AST for src/examples/basic.runes:
Program
  VarDecl name='x' is_const=false is_volatile=false
    TypeExpr kind=named name='i32'
    Init:
      IntLiteral value=5
  VarDecl name='y' is_const=false is_volatile=false
    TypeExpr kind=named name='i64'
    Init:
      IntLiteral value=10
  VarDecl name='pi' is_const=false is_volatile=false
    TypeExpr kind=named name='f32'
    Init:
      FloatLiteral value=3.140000
  VarDecl name='flag' is_const=false is_volatile=false
    TypeExpr kind=named name='bool'
    Init:
      BoolLiteral value=true
  VarDecl name='name' is_const=false is_volatile=false
    TypeExpr kind=named name='str'
    Init:
      StringLiteral value="runes"
  VarDecl name='flags' is_const=false is_volatile=false
    TypeExpr kind=named name='u8'
    Init:
      IntLiteral value=255
  VarDecl name='addr' is_const=false is_volatile=false
    TypeExpr kind=named name='u64'
    Init:
      IntLiteral value=-140737488355328
  VarDecl name='MAX' is_const=true is_volatile=false
    TypeExpr kind=named name='i32'
    Init:
      IntLiteral value=512
  VarDecl name='mmio' is_const=false is_volatile=false
    TypeExpr kind=pointer
      TypeExpr kind=named name='u32'
    Init:
      IndexExpr
        Target:
          IntLiteral value=268435456
        Index:
          IntLiteral value=5
  VarDecl name='nums' is_const=false is_volatile=false
    TypeExpr kind=named name='i32'
    Init:
      IndexExpr
        Target:
          ArrayLiteral
            IntLiteral value=1
            IntLiteral value=2
            IntLiteral value=3
            IntLiteral value=4
            IntLiteral value=5
        Index:
          IntLiteral value=4
  VarDecl name='rgba' is_const=false is_volatile=false
    TypeExpr kind=named name='u8'
    Init:
      IndexExpr
        Target:
          ArrayLiteral
            IntLiteral value=255
            IntLiteral value=0
            IntLiteral value=128
            IntLiteral value=255
        Index:
          IntLiteral value=512
  VarDecl name='entries' is_const=false is_volatile=false
    TypeExpr kind=named name='u64'
    Init:
      ArrayLiteral
  VarDecl name='uart' is_const=false is_volatile=true
    TypeExpr kind=pointer
      TypeExpr kind=named name='u32'
    Init:
      IntLiteral value=268435456

## controlflows.runes
AST for src/examples/controlflows.runes:
Program
  FuncDecl name='control' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
    Body:
      Block
        IfStmt
          Condition:
            BinaryExpr op=>
              Identifier name='x'
              IntLiteral value=0
          Then:
            Block
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  StringLiteral value="positive"
          Else:
            IfStmt
              Condition:
                BinaryExpr op=<
                  Identifier name='x'
                  IntLiteral value=0
              Then:
                Block
                  CallExpr
                    Callee:
                      Identifier name='print'
                    Args:
                      StringLiteral value="negative"
              Else:
                Block
                  CallExpr
                    Callee:
                      Identifier name='print'
                    Args:
                      StringLiteral value="zero"
        VarDecl name='label' is_const=false is_volatile=false
          TypeExpr kind=named name='str'
          Init:
            IfStmt
              Condition:
                BinaryExpr op=>
                  Identifier name='x'
                  IntLiteral value=0
              Then:
                Block
                  StringLiteral value="pos"
              Else:
                Block
                  StringLiteral value="neg"
        WhileStmt
          Condition:
            BinaryExpr op=>
              Identifier name='x'
              IntLiteral value=0
          Body:
            Block
              Assign
                Target:
                  Identifier name='x'
                Value:
                  BinaryExpr op=-
                    Identifier name='x'
                    IntLiteral value=1
        LoopStmt
          Body:
            Block
              IfStmt
                Condition:
                  BinaryExpr op===
                    Identifier name='x'
                    IntLiteral value=0
                Then:
                  Block
                    Node kind=21 (printing not fully implemented)
              Assign
                Target:
                  Identifier name='x'
                Value:
                  BinaryExpr op=-
                    Identifier name='x'
                    IntLiteral value=1
        ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
          Iter:
            RangeExpr inclusive=false
              Start:
                IntLiteral value=0
              End:
                IntLiteral value=10
          Body:
            Block
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  Identifier name='i'
        ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
          Iter:
            RangeExpr inclusive=true
              Start:
                IntLiteral value=0
              End:
                IntLiteral value=10
          Body:
            Block
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  Identifier name='i'
        VarDecl name='nums' is_const=false is_volatile=false
          TypeExpr kind=array
            TypeExpr kind=named name='i32'
            Size:
              IntLiteral value=5
          Init:
            ArrayLiteral
              IntLiteral value=1
              IntLiteral value=2
              IntLiteral value=3
              IntLiteral value=4
              IntLiteral value=5
        ForStmt cap_kind=0 cap_value='n' cap_index='(null)'
          Iter:
            Identifier name='nums'
          Body:
            Block
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  Identifier name='n'
        ForStmt cap_kind=2 cap_value='n' cap_index='i'
          Iter:
            Identifier name='nums'
          Body:
            Block
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  Identifier name='i'
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  Identifier name='n'
        ForStmt cap_kind=1 cap_value='n' cap_index='(null)'
          Iter:
            Identifier name='nums'
          Body:
            Block
              Assign
                Target:
                  UnaryExpr op=*
                    Identifier name='n'
                Value:
                  BinaryExpr op=*
                    UnaryExpr op=*
                      Identifier name='n'
                    IntLiteral value=2
## errors.runes
raul@raul:~$ ./ast_tool src/examples/errors.runes
AST for src/examples/errors.runes:
Program
  Node kind=10 (printing not fully implemented)
  Node kind=10 (printing not fully implemented)
  FuncDecl name='divide' realm=stack is_pub=false is_main=false
    Params:
      Param name='a'
        TypeExpr kind=named name='f32'
      Param name='b'
        TypeExpr kind=named name='f32'
    Return name='result'
      TypeExpr kind=fallible
        TypeExpr kind=named name='f32'
    Body:
      Block
        IfStmt
          Condition:
            BinaryExpr op===
              Identifier name='b'
              FloatLiteral value=0.000000
          Then:
            Block
              Assign
                Target:
                  Identifier name='result'
                Value:
                  ErrorExpr
                    Identifier name='MathError'
                    Identifier name='DivByZero'
          Else:
            Block
              Assign
                Target:
                  Identifier name='result'
                Value:
                  BinaryExpr op=/
                    Identifier name='a'
                    Identifier name='b'
  FuncDecl name='run' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='f32'
    Body:
      Block
        VarDecl name='val' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='divide'
                Args:
                  FloatLiteral value=10.000000
                  FloatLiteral value=2.000000
        Assign
          Target:
            Identifier name='r'
          Value:
            BinaryExpr op=*
              Identifier name='val'
              FloatLiteral value=2.000000
  FuncDecl name='safe_run' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='val' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            CatchExpr err_name='e'
              Expr:
                CallExpr
                  Callee:
                    Identifier name='divide'
                  Args:
                    FloatLiteral value=10.000000
                    FloatLiteral value=0.000000
              Handler:
                Block
                  CallExpr
                    Callee:
                      Identifier name='print'
                    Args:
                      StringLiteral value="error"
                  ReturnStmt
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            Identifier name='val'
  FuncDecl name='safe_default' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='val' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            CatchExpr err_name='(null)'
              Expr:
                CallExpr
                  Callee:
                    Identifier name='divide'
                  Args:
                    FloatLiteral value=10.000000
                    FloatLiteral value=0.000000
              Handler:
                FloatLiteral value=0.000000
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            Identifier name='val'
  FuncDecl name='chained' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='val' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            CatchExpr err_name='(null)'
              Expr:
                CatchExpr err_name='(null)'
                  Expr:
                    CallExpr
                      Callee:
                        Identifier name='divide'
                      Args:
                        FloatLiteral value=10.000000
                        FloatLiteral value=0.000000
                  Handler:
                    CallExpr
                      Callee:
                        Identifier name='divide'
                      Args:
                        FloatLiteral value=1.000000
                        FloatLiteral value=1.000000
              Handler:
                FloatLiteral value=0.000000
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            Identifier name='val'
raul@raul:~$

## hello.runes
AST for src/examples/hello.runes:
Program
  Node kind=12 (printing not fully implemented)
  CallExpr
    Callee:
      Identifier name='print'
    Args:
      StringLiteral value="hello world"
raul@raul:~$

## memory.runes
[Error] src/examples/memory.runes:24:6: expected an expression
[Error] src/examples/memory.runes:25:5: expected variable name
AST for src/examples/memory.runes:
Program
  FuncDecl name='build' realm=regional is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=pointer
        TypeExpr kind=named name='PageTable'
    Body:
      Block
        VarDecl name='t' is_const=false is_volatile=false
          TypeExpr kind=named name='PageTable'
          Init:
            CallExpr
              Callee:
                Identifier name='PageTable'
              Args:
                NamedArg name='entries'
                  ArrayLiteral
        Assign
          Target:
            Identifier name='r'
          Value:
            PromoteExpr target=dynamic
              UnaryExpr op=&
                Identifier name='t'
  FuncDecl name='parse_both' realm=stack is_pub=false is_main=false
    Params:
      Param name='src'
        TypeExpr kind=named name='str'
    Return name='r'
      TypeExpr kind=tuple
        TypeExpr kind=pointer
          TypeExpr kind=named name='Node'
        TypeExpr kind=named name='i32'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            TupleExpr
              CallExpr
                Callee:
                  Identifier name='build'
              IntLiteral value=42
  FuncDecl name='zero_page' realm=stack is_pub=false is_main=false
    Params:
      Param name='addr'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
    Body:
      Block
        Node kind=25 (printing not fully implemented)

## os.runes
raul@raul:~$ ./ast_tool src/examples/os.runes
[Error] src/examples/os.runes:12:1: expected field name
[Error] src/examples/os.runes:27:1: expected an expression
[Error] src/examples/os.runes:33:42: expected ')' after arguments
[Error] src/examples/os.runes:33:54: expected variable name
[Error] src/examples/os.runes:33:59: expected an expression
[Error] src/examples/os.runes:39:9: expected an expression
[Error] src/examples/os.runes:48:1: expected an expression
AST for src/examples/os.runes:
Program
  Node kind=12 (printing not fully implemented)
  Node kind=10 (printing not fully implemented)
  FuncDecl name='new' realm=regional is_pub=false is_main=false
    Return name='t'
      TypeExpr kind=named name='PageTable'
    Body:
      Block
        Assign
          Target:
            Identifier name='t'
          Value:
            CallExpr
              Callee:
                Identifier name='PageTable'
              Args:
                NamedArg name='entries'
                  ArrayLiteral
  FuncDecl name='map' realm=stack is_pub=false is_main=false
    Params:
      Param name='self'
      Param name='vaddr'
        TypeExpr kind=named name='u64'
      Param name='paddr'
        TypeExpr kind=named name='u64'
      Param name='flags'
        TypeExpr kind=named name='u64'
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='void'
    Body:
      Block
        VarDecl name='idx' is_const=false is_volatile=false
          TypeExpr kind=named name='u64'
          Init:
            BinaryExpr op=&
              BinaryExpr op=>>
                Identifier name='vaddr'
                IntLiteral value=12
              IntLiteral value=511
        IfStmt
          Condition:
            BinaryExpr op=!=
              IndexExpr
                Target:
                  FieldExpr field='entries'
                    Identifier name='self'
                Index:
                  Identifier name='idx'
              IntLiteral value=0
          Then:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  ErrorExpr
                    Identifier name='MapError'
                    Identifier name='AlreadyMapped'
          Else:
            Block
              Assign
                Target:
                  IndexExpr
                    Target:
                      FieldExpr field='entries'
                        Identifier name='self'
                    Index:
                      Identifier name='idx'
                Value:
                  BinaryExpr op=|
                    Identifier name='paddr'
                    Identifier name='flags'
  FuncDecl name='kernel_main' realm=stack is_pub=true is_main=false
    Body:
      Block
        FuncDecl name='setup' realm=regional is_pub=false is_main=false
          Body:
            Block
              VarDecl name='pml4' is_const=false is_volatile=false
                TypeExpr kind=named name='PageTable'
                Init:
                  CallExpr
                    Callee:
                      FieldExpr field='new'
                        Identifier name='PageTable'
              VarDecl name='pdpt' is_const=false is_volatile=false
                TypeExpr kind=named name='PageTable'
                Init:
                  CallExpr
                    Callee:
                      FieldExpr field='new'
                        Identifier name='PageTable'
              VarDecl name='(null)' is_const=false is_volatile=false
                TypeExpr kind=named name='u64'
        CallExpr
          Callee:
            Identifier name='setup'
        Node kind=25 (printing not fully implemented)
  FuncDecl name='run_userspace' realm=gc is_pub=false is_main=false
    Body:
      Block
        VarDecl name='t' is_const=false is_volatile=false
          TypeExpr kind=named name='Task'
          Init:
            CallExpr
              Callee:
                FieldExpr field='spawn'
                  Identifier name='Task'
              Args:
                Identifier name='shell_main'
        CallExpr
          Callee:
            FieldExpr field='add'
              Identifier name='scheduler'
          Args:
            Identifier name='t'
  CallExpr
    Callee:
      Identifier name='run_userspace'

## schemas.runes

raul@raul:~$ ./ast_tool src/examples/schemas.runes
AST for src/examples/schemas.runes:
Program
  Node kind=6 (printing not fully implemented)
  Node kind=6 (printing not fully implemented)
  Node kind=6 (printing not fully implemented)
  FuncDecl name='handle' realm=stack is_pub=false is_main=false
    Params:
      Param name='shoe'
        TypeExpr kind=named name='Shoe'
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            FieldExpr field='brand'
              Identifier name='shoe'
        Assign
          Target:
            Identifier name='j'
          Value:
            CastExpr kind=0
              Expr:
                Identifier name='shoe'
              Target Type:
                TypeExpr kind=J
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            CallExpr
              Callee:
                FieldExpr field='string'
                  Identifier name='j'
raul@raul:~$

## testfuncs.runes
raul@raul:~$ ./ast_tool src/examples/testfuncs.runes
AST for src/examples/testfuncs.runes:
Program
  FuncDecl name='greet' realm=stack is_pub=false is_main=false
    Params:
      Param name='name'
        TypeExpr kind=named name='str'
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            Identifier name='name'
  FuncDecl name='add' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
      Param name='y'
        TypeExpr kind=named name='i32'
    Return name='result'
      TypeExpr kind=named name='i32'
    Body:
      Block
        Assign
          Target:
            Identifier name='result'
          Value:
            BinaryExpr op=+
              Identifier name='x'
              Identifier name='y'
  FuncDecl name='square' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            BinaryExpr op=*
              Identifier name='x'
              Identifier name='x'
  FuncDecl name='alloc_buf' realm=dynamic is_pub=false is_main=false
    Params:
      Param name='size'
        TypeExpr kind=named name='u64'
    Return name='ptr'
      TypeExpr kind=pointer
        TypeExpr kind=named name='u8'
    Body:
      Block
        Assign
          Target:
            Identifier name='ptr'
          Value:
            CallExpr
              Callee:
                Identifier name='raw_alloc'
              Args:
                Identifier name='size'
  FuncDecl name='make_table' realm=regional is_pub=false is_main=false
    Return name='t'
      TypeExpr kind=named name='PageTable'
    Body:
      Block
        Assign
          Target:
            Identifier name='t'
          Value:
            CallExpr
              Callee:
                Identifier name='PageTable'
              Args:
                NamedArg name='entries'
                  ArrayLiteral
  FuncDecl name='run_shell' realm=gc is_pub=false is_main=false
    Params:
      Param name='input'
        TypeExpr kind=named name='str'
    Return name='result'
      TypeExpr kind=named name='str'
    Body:
      Block
        Assign
          Target:
            Identifier name='result'
          Value:
            Identifier name='input'
  FuncDecl name='strict' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            Identifier name='x'
  FuncDecl name='main' realm=stack is_pub=false is_main=true
    Body:
      Block
        FuncDecl name='setup' realm=regional is_pub=false is_main=false
          Body:
            Block
              VarDecl name='x' is_const=false is_volatile=false
                TypeExpr kind=named name='i32'
                Init:
                  IntLiteral value=5
        FuncDecl name='run' realm=gc is_pub=false is_main=false
          Body:
            Block
              VarDecl name='y' is_const=false is_volatile=false
                TypeExpr kind=named name='i32'
                Init:
                  IntLiteral value=10
        CallExpr
          Callee:
            Identifier name='setup'
        CallExpr
          Callee:
            Identifier name='run'
raul@raul:~$

## types.runes

raul@raul:~$ ./ast_tool src/examples/types.runes
[Error] src/examples/types.runes:9:1: expected field name
[Error] src/examples/types.runes:16:1: expected field name
[Error] src/examples/types.runes:33:38: expected ')' after arguments
[Error] src/examples/types.runes:33:56: expected an expression
[Error] src/examples/types.runes:37:26: expected ')' after arguments
[Error] src/examples/types.runes:37:32: expected an expression
[Error] src/examples/types.runes:45:1: expected an expression
[Error] src/examples/types.runes:48:26: expected '(' after function name
AST for src/examples/types.runes:
Program
  TypeDecl name='Vec2' is_pub=false
    FieldDecl name='x' is_volatile=false
      TypeExpr kind=named name='f32'
    FieldDecl name='y' is_volatile=false
      TypeExpr kind=named name='f32'
  Node kind=4 (printing not fully implemented)
  Node kind=8 (printing not fully implemented)
  Node kind=9 (printing not fully implemented)
raul@raul:~$
