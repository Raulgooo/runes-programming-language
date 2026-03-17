okay, lets review the tests you gave me:
## test 01:
raul@raul:~$ ./ast_tool src/tests/samples/01_variables.runes
AST for src/tests/samples/01_variables.runes:
Program
  VarDecl name='tiny' is_const=false is_volatile=false
    TypeExpr kind=named name='i8'
    Init:
      UnaryExpr op=-
        IntLiteral value=1
  VarDecl name='small' is_const=false is_volatile=false
    TypeExpr kind=named name='i16'
    Init:
      IntLiteral value=256
  VarDecl name='x' is_const=false is_volatile=false
    TypeExpr kind=named name='i32'
    Init:
      IntLiteral value=5
  VarDecl name='y' is_const=false is_volatile=false
    TypeExpr kind=named name='i64'
    Init:
      IntLiteral value=10
  VarDecl name='flags' is_const=false is_volatile=false
    TypeExpr kind=named name='u8'
    Init:
      IntLiteral value=255
  VarDecl name='port' is_const=false is_volatile=false
    TypeExpr kind=named name='u16'
    Init:
      IntLiteral value=8080
  VarDecl name='mask' is_const=false is_volatile=false
    TypeExpr kind=named name='u32'
    Init:
      IntLiteral value=16711935
  VarDecl name='addr' is_const=false is_volatile=false
    TypeExpr kind=named name='u64'
    Init:
      IntLiteral value=18446603336221196288
  VarDecl name='pi' is_const=false is_volatile=false
    TypeExpr kind=named name='f32'
    Init:
      FloatLiteral value=3.141590
  VarDecl name='e' is_const=false is_volatile=false
    TypeExpr kind=named name='f64'
    Init:
      FloatLiteral value=2.718282
  VarDecl name='running' is_const=false is_volatile=false
    TypeExpr kind=named name='bool'
    Init:
      BoolLiteral value=true
  VarDecl name='done' is_const=false is_volatile=false
    TypeExpr kind=named name='bool'
    Init:
      BoolLiteral value=false
  VarDecl name='newline' is_const=false is_volatile=false
    TypeExpr kind=named name='char'
    Init:
      CharLiteral codepoint=U+000A
  VarDecl name='letter' is_const=false is_volatile=false
    TypeExpr kind=named name='char'
    Init:
      CharLiteral codepoint=U+0041
  VarDecl name='tag' is_const=false is_volatile=false
    TypeExpr kind=named name='str'
    Init:
      StringLiteral value="runes"
  VarDecl name='empty' is_const=false is_volatile=false
    TypeExpr kind=named name='str'
    Init:
      StringLiteral value=""
  Assign
    Target:
      Identifier name='inferred_int'
    Value:
      IntLiteral value=42
  Assign
    Target:
      Identifier name='inferred_float'
    Value:
      FloatLiteral value=2.710000
  Assign
    Target:
      Identifier name='inferred_str'
    Value:
      StringLiteral value="hello, world"
  Assign
    Target:
      Identifier name='inferred_bool'
    Value:
      BoolLiteral value=true
  VarDecl name='MAX_PAGES' is_const=true is_volatile=false
    TypeExpr kind=named name='i32'
    Init:
      IntLiteral value=512
  VarDecl name='KERNEL_BASE' is_const=true is_volatile=false
    TypeExpr kind=named name='u64'
    Init:
      IntLiteral value=18446603336221196288
  VarDecl name='NULL_BYTE' is_const=true is_volatile=false
    TypeExpr kind=named name='u8'
    Init:
      IntLiteral value=0
  VarDecl name='GRAVITY' is_const=true is_volatile=false
    TypeExpr kind=named name='f32'
    Init:
      FloatLiteral value=9.810000
  VarDecl name='LIMIT' is_const=true is_volatile=false
    Init:
      IntLiteral value=1024
  VarDecl name='VERSION' is_const=true is_volatile=false
    Init:
      StringLiteral value="v0.1"
  VarDecl name='DEBUG_MODE' is_const=true is_volatile=false
    Init:
      BoolLiteral value=false
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
  VarDecl name='rgba' is_const=false is_volatile=false
    TypeExpr kind=array
      TypeExpr kind=named name='u8'
      Size:
        IntLiteral value=4
    Init:
      ArrayLiteral
        IntLiteral value=255
        IntLiteral value=0
        IntLiteral value=128
        IntLiteral value=255
  VarDecl name='pml4' is_const=false is_volatile=false
    TypeExpr kind=array
      TypeExpr kind=named name='u64'
      Size:
        IntLiteral value=512
    Init:
      ArrayLiteral
  VarDecl name='zero_page' is_const=false is_volatile=false
    TypeExpr kind=array
      TypeExpr kind=named name='u8'
      Size:
        IntLiteral value=256
    Init:
      ArrayLiteral
  VarDecl name='weights' is_const=false is_volatile=false
    TypeExpr kind=array
      TypeExpr kind=named name='f32'
      Size:
        IntLiteral value=8
    Init:
      ArrayLiteral
        FloatLiteral value=0.100000
        FloatLiteral value=0.200000
        FloatLiteral value=0.300000
        FloatLiteral value=0.400000
        FloatLiteral value=0.500000
        FloatLiteral value=0.600000
        FloatLiteral value=0.700000
        FloatLiteral value=0.800000
  Assign
    Target:
      IndexExpr
        Target:
          Identifier name='nums'
        Index:
          IntLiteral value=0
    Value:
      IntLiteral value=10
  VarDecl name='first' is_const=false is_volatile=false
    TypeExpr kind=named name='i32'
    Init:
      IndexExpr
        Target:
          Identifier name='nums'
        Index:
          IntLiteral value=0
  VarDecl name='middle' is_const=false is_volatile=false
    TypeExpr kind=named name='i32'
    Init:
      IndexExpr
        Target:
          Identifier name='nums'
        Index:
          IntLiteral value=2
  VarDecl name='alpha' is_const=false is_volatile=false
    TypeExpr kind=named name='u8'
    Init:
      IndexExpr
        Target:
          Identifier name='rgba'
        Index:
          IntLiteral value=3
  VarDecl name='raw_buf' is_const=false is_volatile=false
    TypeExpr kind=pointer
      TypeExpr kind=named name='u8'
    Init:
      CastExpr kind=0
        Expr:
          IntLiteral value=0
        Target Type:
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
  VarDecl name='table' is_const=false is_volatile=false
    TypeExpr kind=pointer
      TypeExpr kind=named name='u64'
    Init:
      CastExpr kind=0
        Expr:
          IntLiteral value=18446462598732840960
        Target Type:
          TypeExpr kind=pointer
            TypeExpr kind=named name='u64'
  VarDecl name='uart' is_const=false is_volatile=true
    TypeExpr kind=pointer
      TypeExpr kind=named name='u32'
    Init:
      CastExpr kind=0
        Expr:
          IntLiteral value=268435456
        Target Type:
          TypeExpr kind=pointer
            TypeExpr kind=named name='u32'
  VarDecl name='pic' is_const=false is_volatile=true
    TypeExpr kind=pointer
      TypeExpr kind=named name='u8'
    Init:
      CastExpr kind=0
        Expr:
          IntLiteral value=4273995776
        Target Type:
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
raul@raul:~$
## test 02:  
raul@raul:~$ ./ast_tool src/tests/samples/02_functions.runes
[Error] src/tests/samples/02_functions.runes:197:14: expected an expression
[Error] src/tests/samples/02_functions.runes:198:11: expected an expression
[Error] src/tests/samples/02_functions.runes:198:32: expected ')' after arguments
[Error] src/tests/samples/02_functions.runes:198:36: expected an expression
[Error] src/tests/samples/02_functions.runes:209:42: expected ')' after arguments
[Error] src/tests/samples/02_functions.runes:209:54: expected variable name
[Error] src/tests/samples/02_functions.runes:209:59: expected an expression
[Error] src/tests/samples/02_functions.runes:217:30: expected ')' after arguments
[Error] src/tests/samples/02_functions.runes:217:43: expected an expression
AST for src/tests/samples/02_functions.runes:
Program
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
            BinaryExpr op=+
              StringLiteral value="hello, "
              Identifier name='name'
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
  FuncDecl name='double' realm=stack is_pub=false is_main=false
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
              IntLiteral value=2
  FuncDecl name='clamp' realm=stack is_pub=false is_main=false
    Params:
      Param name='val'
        TypeExpr kind=named name='i32'
      Param name='lo'
        TypeExpr kind=named name='i32'
      Param name='hi'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        IfStmt
          Condition:
            BinaryExpr op=<
              Identifier name='val'
              Identifier name='lo'
          Then:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  Identifier name='lo'
          Else:
            IfStmt
              Condition:
                BinaryExpr op=>
                  Identifier name='val'
                  Identifier name='hi'
              Then:
                Block
                  Assign
                    Target:
                      Identifier name='r'
                    Value:
                      Identifier name='hi'
              Else:
                Block
                  Assign
                    Target:
                      Identifier name='r'
                    Value:
                      Identifier name='val'
  FuncDecl name='zero_byte' realm=stack is_pub=false is_main=false
    Params:
      Param name='p'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
    Body:
      Block
        Assign
          Target:
            UnaryExpr op=*
              Identifier name='p'
          Value:
            IntLiteral value=0
  FuncDecl name='read_byte' realm=stack is_pub=false is_main=false
    Params:
      Param name='p'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
    Return name='r'
      TypeExpr kind=named name='u8'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            UnaryExpr op=*
              Identifier name='p'
  FuncDecl name='fast_hash' realm=stack is_pub=false is_main=false
    Params:
      Param name='data'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            IntLiteral value=0
        ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
          Iter:
            RangeExpr inclusive=false
              Start:
                IntLiteral value=0
              End:
                Identifier name='len'
          Body:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  BinaryExpr op=^
                    Identifier name='r'
                    UnaryExpr op=*
                      CastExpr kind=0
                        Expr:
                          BinaryExpr op=+
                            Identifier name='data'
                            Identifier name='i'
                        Target Type:
                          TypeExpr kind=named name='u64'
              Assign
                Target:
                  Identifier name='r'
                Value:
                  BinaryExpr op=*
                    Identifier name='r'
                    IntLiteral value=11400714819323198485
  FuncDecl name='min' realm=stack is_pub=false is_main=false
    Params:
      Param name='a'
        TypeExpr kind=named name='i32'
      Param name='b'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            IfStmt
              Condition:
                BinaryExpr op=<
                  Identifier name='a'
                  Identifier name='b'
              Then:
                Block
                  Identifier name='a'
              Else:
                Block
                  Identifier name='b'
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
  FuncDecl name='alloc_zeroed' realm=dynamic is_pub=false is_main=false
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
        CallExpr
          Callee:
            Identifier name='memset'
          Args:
            Identifier name='ptr'
            IntLiteral value=0
            Identifier name='size'
  FuncDecl name='init_driver' realm=dynamic is_pub=false is_main=false
    Params:
      Param name='name'
        TypeExpr kind=named name='str'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        FuncDecl name='validate' realm=stack is_pub=false is_main=false
          Params:
            Param name='s'
              TypeExpr kind=named name='str'
          Return name='ok'
            TypeExpr kind=named name='bool'
          Body:
            Block
              Assign
                Target:
                  Identifier name='ok'
                Value:
                  BinaryExpr op=>
                    FieldExpr field='len'
                      Identifier name='s'
                    IntLiteral value=0
        FuncDecl name='load_config' realm=gc is_pub=false is_main=false
          Params:
            Param name='s'
              TypeExpr kind=named name='str'
          Return name='cfg'
            TypeExpr kind=pointer
              TypeExpr kind=named name='Config'
          Body:
            Block
              Assign
                Target:
                  Identifier name='cfg'
                Value:
                  CallExpr
                    Callee:
                      Identifier name='parse_config'
                    Args:
                      Identifier name='s'
        IfStmt
          Condition:
            CallExpr
              Callee:
                Identifier name='validate'
              Args:
                Identifier name='name'
          Then:
            Block
              VarDecl name='c' is_const=false is_volatile=false
                TypeExpr kind=pointer
                  TypeExpr kind=named name='Config'
                Init:
                  CallExpr
                    Callee:
                      Identifier name='load_config'
                    Args:
                      Identifier name='name'
              Assign
                Target:
                  Identifier name='r'
                Value:
                  FieldExpr field='id'
                    Identifier name='c'
          Else:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  UnaryExpr op=-
                    IntLiteral value=1
  FuncDecl name='make_page_table' realm=regional is_pub=false is_main=false
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
  FuncDecl name='build_tables' realm=regional is_pub=false is_main=false
    Body:
      Block
        FuncDecl name='zero' realm=stack is_pub=false is_main=false
          Params:
            Param name='p'
              TypeExpr kind=pointer
                TypeExpr kind=named name='u8'
          Body:
            Block
              Assign
                Target:
                  UnaryExpr op=*
                    Identifier name='p'
                Value:
                  IntLiteral value=0
        VarDecl name='pml4' is_const=false is_volatile=false
          TypeExpr kind=named name='PageTable'
          Init:
            CallExpr
              Callee:
                Identifier name='PageTable'
              Args:
                NamedArg name='entries'
                  ArrayLiteral
        VarDecl name='pdpt' is_const=false is_volatile=false
          TypeExpr kind=named name='PageTable'
          Init:
            CallExpr
              Callee:
                Identifier name='PageTable'
              Args:
                NamedArg name='entries'
                  ArrayLiteral
        CallExpr
          Callee:
            Identifier name='zero'
          Args:
            UnaryExpr op=&
              CastExpr kind=0
                Expr:
                  Identifier name='pml4'
                Target Type:
                  TypeExpr kind=pointer
                    TypeExpr kind=named name='u8'
        CallExpr
          Callee:
            Identifier name='zero'
          Args:
            UnaryExpr op=&
              CastExpr kind=0
                Expr:
                  Identifier name='pdpt'
                Target Type:
                  TypeExpr kind=pointer
                    TypeExpr kind=named name='u8'
  FuncDecl name='build_promoted' realm=regional is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=tuple
        TypeExpr kind=pointer
          TypeExpr kind=named name='PageTable'
        TypeExpr kind=pointer
          TypeExpr kind=named name='PageTable'
    Body:
      Block
        VarDecl name='pml4' is_const=false is_volatile=false
          TypeExpr kind=named name='PageTable'
          Init:
            CallExpr
              Callee:
                Identifier name='PageTable'
              Args:
                NamedArg name='entries'
                  ArrayLiteral
        VarDecl name='pdpt' is_const=false is_volatile=false
          TypeExpr kind=named name='PageTable'
          Init:
            CallExpr
              Callee:
                Identifier name='PageTable'
              Args:
                NamedArg name='entries'
                  ArrayLiteral
        VarDecl name='pml4_h' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='PageTable'
          Init:
            PromoteExpr target=dynamic
              UnaryExpr op=&
                Identifier name='pml4'
        VarDecl name='pdpt_h' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='PageTable'
          Init:
            PromoteExpr target=dynamic
              UnaryExpr op=&
                Identifier name='pdpt'
        Assign
          Target:
            Identifier name='r'
          Value:
            TupleExpr
              Identifier name='pml4_h'
              Identifier name='pdpt_h'
  FuncDecl name='build_gc' realm=regional is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=pointer
        TypeExpr kind=named name='Node'
    Body:
      Block
        VarDecl name='n' is_const=false is_volatile=false
          TypeExpr kind=named name='Node'
          Init:
            CallExpr
              Callee:
                Identifier name='Node'
              Args:
                NamedArg name='val'
                  IntLiteral value=99
                NamedArg name='next'
                  CastExpr kind=0
                    Expr:
                      IntLiteral value=0
                    Target Type:
                      TypeExpr kind=pointer
                        TypeExpr kind=named name='Node'
        Assign
          Target:
            Identifier name='r'
          Value:
            PromoteExpr target=gc
              UnaryExpr op=&
                Identifier name='n'
  FuncDecl name='run_shell' realm=gc is_pub=false is_main=false
    Params:
      Param name='input'
        TypeExpr kind=named name='str'
    Return name='result'
      TypeExpr kind=pointer
        TypeExpr kind=named name='Token'
    Body:
      Block
        Assign
          Target:
            Identifier name='result'
          Value:
            CallExpr
              Callee:
                Identifier name='tokenize'
              Args:
                Identifier name='input'
  FuncDecl name='run_pipeline' realm=gc is_pub=false is_main=false
    Params:
      Param name='src'
        TypeExpr kind=named name='str'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        FuncDecl name='validate' realm=stack is_pub=false is_main=false
          Params:
            Param name='s'
              TypeExpr kind=named name='str'
          Return name='ok'
            TypeExpr kind=named name='bool'
          Body:
            Block
              Assign
                Target:
                  Identifier name='ok'
                Value:
                  BinaryExpr op=>
                    FieldExpr field='len'
                      Identifier name='s'
                    IntLiteral value=0
        FuncDecl name='parse' realm=gc is_pub=false is_main=false
          Params:
            Param name='s'
              TypeExpr kind=named name='str'
          Return name='ast'
            TypeExpr kind=pointer
              TypeExpr kind=named name='Node'
          Body:
            Block
              Assign
                Target:
                  Identifier name='ast'
                Value:
                  CallExpr
                    Callee:
                      Identifier name='build_ast'
                    Args:
                      Identifier name='s'
        IfStmt
          Condition:
            CallExpr
              Callee:
                Identifier name='validate'
              Args:
                Identifier name='src'
          Then:
            Block
              VarDecl name='tree' is_const=false is_volatile=false
                TypeExpr kind=pointer
                  TypeExpr kind=named name='Node'
                Init:
                  CallExpr
                    Callee:
                      Identifier name='parse'
                    Args:
                      Identifier name='src'
              Assign
                Target:
                  Identifier name='r'
                Value:
                  CallExpr
                    Callee:
                      Identifier name='eval'
                    Args:
                      Identifier name='tree'
          Else:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  UnaryExpr op=-
                    IntLiteral value=1
  FuncDecl name='make_node' realm=flex is_pub=false is_main=false
    Params:
      Param name='val'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=pointer
        TypeExpr kind=named name='Node'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            CallExpr
              Callee:
                Identifier name='alloc'
              Args:
                CallExpr
                  Callee:
                    Identifier name='sizeof'
                  Args:
                    Identifier name='Node'
        Assign
          Target:
            FieldExpr field='val'
              Identifier name='r'
          Value:
            Identifier name='val'
        Assign
          Target:
            FieldExpr field='next'
              Identifier name='r'
          Value:
            CastExpr kind=0
              Expr:
                IntLiteral value=0
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='Node'
  FuncDecl name='copy_str' realm=flex is_pub=false is_main=false
    Params:
      Param name='s'
        TypeExpr kind=named name='str'
    Return name='r'
      TypeExpr kind=pointer
        TypeExpr kind=named name='u8'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            CallExpr
              Callee:
                Identifier name='alloc'
              Args:
                BinaryExpr op=+
                  FieldExpr field='len'
                    Identifier name='s'
                  IntLiteral value=1
        CallExpr
          Callee:
            Identifier name='memcpy'
          Args:
            Identifier name='r'
            FieldExpr field='ptr'
              Identifier name='s'
            FieldExpr field='len'
              Identifier name='s'
        Assign
          Target:
            UnaryExpr op=*
              BinaryExpr op=+
                Identifier name='r'
                FieldExpr field='len'
                  Identifier name='s'
          Value:
            IntLiteral value=0
  FuncDecl name='parse' realm=stack is_pub=false is_main=false
    Params:
      Param name='src'
        TypeExpr kind=named name='str'
    Return name='r'
      TypeExpr kind=tuple
        TypeExpr kind=pointer
          TypeExpr kind=named name='Node'
        TypeExpr kind=array
          TypeExpr kind=named name='Error'
          Size:
            IntLiteral value=512
    Body:
      Block
        VarDecl name='ast' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='Node'
          Init:
            CallExpr
              Callee:
                Identifier name='build_ast'
              Args:
                Identifier name='src'
        VarDecl name='errs' is_const=false is_volatile=false
          TypeExpr kind=array
            TypeExpr kind=named name='Error'
            Size:
              IntLiteral value=512
          Init:
            CallExpr
              Callee:
                Identifier name='collect_errors'
              Args:
                Identifier name='src'
        Assign
          Target:
            Identifier name='r'
          Value:
            TupleExpr
              Identifier name='ast'
              Identifier name='errs'
  FuncDecl name='minmax' realm=stack is_pub=false is_main=false
    Params:
      Param name='a'
        TypeExpr kind=named name='i32'
      Param name='b'
        TypeExpr kind=named name='i32'
      Param name='c'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=tuple
        TypeExpr kind=named name='i32'
        TypeExpr kind=named name='i32'
    Body:
      Block
        VarDecl name='lo' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            Identifier name='a'
        VarDecl name='hi' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            Identifier name='a'
        IfStmt
          Condition:
            BinaryExpr op=<
              Identifier name='b'
              Identifier name='lo'
          Then:
            Block
              Assign
                Target:
                  Identifier name='lo'
                Value:
                  Identifier name='b'
        IfStmt
          Condition:
            BinaryExpr op=>
              Identifier name='b'
              Identifier name='hi'
          Then:
            Block
              Assign
                Target:
                  Identifier name='hi'
                Value:
                  Identifier name='b'
        IfStmt
          Condition:
            BinaryExpr op=<
              Identifier name='c'
              Identifier name='lo'
          Then:
            Block
              Assign
                Target:
                  Identifier name='lo'
                Value:
                  Identifier name='c'
        IfStmt
          Condition:
            BinaryExpr op=>
              Identifier name='c'
              Identifier name='hi'
          Then:
            Block
              Assign
                Target:
                  Identifier name='hi'
                Value:
                  Identifier name='c'
        Assign
          Target:
            Identifier name='r'
          Value:
            TupleExpr
              Identifier name='lo'
              Identifier name='hi'
  FuncDecl name='demo_tuples' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='ast' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='Node'
        VarDecl name='errs' is_const=false is_volatile=false
          TypeExpr kind=array
            TypeExpr kind=named name='Error'
            Size:
              IntLiteral value=512
          Init:
            CallExpr
              Callee:
                Identifier name='parse'
              Args:
                StringLiteral value="x = 1 + 2"
        VarDecl name='lo' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
  FuncDecl name='kernel_main' realm=stack is_pub=true is_main=false
    Body:
      Block
        FuncDecl name='setup_paging' realm=regional is_pub=false is_main=false
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
            Identifier name='setup_paging'
        UnsafeBlock
          Body:
            Block
              AsmExpr code='mov %rax, %cr3' output='(none)'
        FuncDecl name='run_userspace' realm=gc is_pub=false is_main=false
          Body:
            Block
              Identifier name='t'
        CallExpr
          Callee:
            Identifier name='run_userspace'
## test 03: 
raul@raul:~$ ./ast_tool src/tests/samples/03_control_flow.runes
AST for src/tests/samples/03_control_flow.runes:
Program
  FuncDecl name='demo_if' realm=stack is_pub=false is_main=false
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
        IfStmt
          Condition:
            BinaryExpr op===
              Identifier name='x'
              IntLiteral value=0
          Then:
            Block
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  StringLiteral value="exactly zero"
        IfStmt
          Condition:
            BinaryExpr op=>
              Identifier name='x'
              IntLiteral value=0
          Then:
            Block
              IfStmt
                Condition:
                  BinaryExpr op=>
                    Identifier name='x'
                    IntLiteral value=100
                Then:
                  Block
                    CallExpr
                      Callee:
                        Identifier name='print'
                      Args:
                        StringLiteral value="very large"
                Else:
                  Block
                    CallExpr
                      Callee:
                        Identifier name='print'
                      Args:
                        StringLiteral value="moderate"
  FuncDecl name='demo_if_expr' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='str'
    Body:
      Block
        VarDecl name='sign' is_const=false is_volatile=false
          TypeExpr kind=named name='str'
          Init:
            IfStmt
              Condition:
                BinaryExpr op=>=
                  Identifier name='x'
                  IntLiteral value=0
              Then:
                Block
                  StringLiteral value="non-negative"
              Else:
                Block
                  StringLiteral value="negative"
        VarDecl name='abs_x' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            IfStmt
              Condition:
                BinaryExpr op=<
                  Identifier name='x'
                  IntLiteral value=0
              Then:
                Block
                  BinaryExpr op=*
                    Identifier name='x'
                    UnaryExpr op=-
                      IntLiteral value=1
              Else:
                Block
                  Identifier name='x'
        Assign
          Target:
            Identifier name='r'
          Value:
            IfStmt
              Condition:
                BinaryExpr op=>
                  Identifier name='x'
                  IntLiteral value=0
              Then:
                Block
                  StringLiteral value="pos"
              Else:
                IfStmt
                  Condition:
                    BinaryExpr op=<
                      Identifier name='x'
                      IntLiteral value=0
                  Then:
                    Block
                      StringLiteral value="neg"
                  Else:
                    Block
                      StringLiteral value="zero"
  FuncDecl name='demo_while' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='running' is_const=false is_volatile=false
          TypeExpr kind=named name='bool'
          Init:
            BoolLiteral value=true
        VarDecl name='ticks' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            IntLiteral value=0
        WhileStmt
          Condition:
            Identifier name='running'
          Body:
            Block
              CallExpr
                Callee:
                  Identifier name='tick'
              Assign
                Target:
                  Identifier name='ticks'
                Value:
                  BinaryExpr op=+
                    Identifier name='ticks'
                    IntLiteral value=1
              IfStmt
                Condition:
                  BinaryExpr op=>=
                    Identifier name='ticks'
                    IntLiteral value=100
                Then:
                  Block
                    Assign
                      Target:
                        Identifier name='running'
                      Value:
                        BoolLiteral value=false
        VarDecl name='n' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            IntLiteral value=1
        WhileStmt
          Condition:
            BinaryExpr op=<
              Identifier name='n'
              IntLiteral value=1024
          Body:
            Block
              Assign
                Target:
                  Identifier name='n'
                Value:
                  BinaryExpr op=*
                    Identifier name='n'
                    IntLiteral value=2
  FuncDecl name='demo_loop' realm=stack is_pub=false is_main=false
    Body:
      Block
        LoopStmt
          Body:
            Block
              IfStmt
                Condition:
                  CallExpr
                    Callee:
                      Identifier name='done'
                Then:
                  Block
                    BreakStmt
        VarDecl name='count' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            IntLiteral value=0
        LoopStmt
          Body:
            Block
              Assign
                Target:
                  Identifier name='count'
                Value:
                  BinaryExpr op=+
                    Identifier name='count'
                    IntLiteral value=1
              IfStmt
                Condition:
                  BinaryExpr op===
                    Identifier name='count'
                    IntLiteral value=10
                Then:
                  Block
                    BreakStmt
              IfStmt
                Condition:
                  BinaryExpr op===
                    Identifier name='count'
                    IntLiteral value=5
                Then:
                  Block
                    CallExpr
                      Callee:
                        Identifier name='print'
                      Args:
                        StringLiteral value="halfway"
        VarDecl name='i' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            IntLiteral value=0
        LoopStmt
          Body:
            Block
              VarDecl name='j' is_const=false is_volatile=false
                TypeExpr kind=named name='i32'
                Init:
                  IntLiteral value=0
              LoopStmt
                Body:
                  Block
                    Assign
                      Target:
                        Identifier name='j'
                      Value:
                        BinaryExpr op=+
                          Identifier name='j'
                          IntLiteral value=1
                    IfStmt
                      Condition:
                        BinaryExpr op===
                          Identifier name='j'
                          IntLiteral value=5
                      Then:
                        Block
                          BreakStmt
              Assign
                Target:
                  Identifier name='i'
                Value:
                  BinaryExpr op=+
                    Identifier name='i'
                    IntLiteral value=1
              IfStmt
                Condition:
                  BinaryExpr op===
                    Identifier name='i'
                    IntLiteral value=3
                Then:
                  Block
                    BreakStmt
  FuncDecl name='demo_for_range' realm=stack is_pub=false is_main=false
    Body:
      Block
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
        ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
          Iter:
            RangeExpr inclusive=false
              Start:
                IntLiteral value=5
              End:
                IntLiteral value=20
          Body:
            Block
              VarDecl name='doubled' is_const=false is_volatile=false
                TypeExpr kind=named name='i32'
                Init:
                  BinaryExpr op=*
                    Identifier name='i'
                    IntLiteral value=2
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  Identifier name='doubled'
  FuncDecl name='demo_for_array' realm=stack is_pub=false is_main=false
    Body:
      Block
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
        VarDecl name='bytes' is_const=false is_volatile=false
          TypeExpr kind=array
            TypeExpr kind=named name='u8'
            Size:
              IntLiteral value=4
          Init:
            ArrayLiteral
              IntLiteral value=222
              IntLiteral value=173
              IntLiteral value=190
              IntLiteral value=239
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
        ForStmt cap_kind=2 cap_value='b' cap_index='i'
          Iter:
            Identifier name='bytes'
          Body:
            Block
              Assign
                Target:
                  UnaryExpr op=*
                    Identifier name='b'
                Value:
                  BinaryExpr op=^
                    UnaryExpr op=*
                      Identifier name='b'
                    CastExpr kind=0
                      Expr:
                        Identifier name='i'
                      Target Type:
                        TypeExpr kind=named name='u8'
  FuncDecl name='demo_nested_loops' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='row' is_const=false is_volatile=false
          TypeExpr kind=array
            TypeExpr kind=named name='i32'
            Size:
              IntLiteral value=4
          Init:
            ArrayLiteral
              IntLiteral value=1
              IntLiteral value=2
              IntLiteral value=3
              IntLiteral value=4
        VarDecl name='col' is_const=false is_volatile=false
          TypeExpr kind=array
            TypeExpr kind=named name='i32'
            Size:
              IntLiteral value=4
          Init:
            ArrayLiteral
              IntLiteral value=10
              IntLiteral value=20
              IntLiteral value=30
              IntLiteral value=40
        ForStmt cap_kind=0 cap_value='r' cap_index='(null)'
          Iter:
            Identifier name='row'
          Body:
            Block
              ForStmt cap_kind=0 cap_value='c' cap_index='(null)'
                Iter:
                  Identifier name='col'
                Body:
                  Block
                    VarDecl name='product' is_const=false is_volatile=false
                      TypeExpr kind=named name='i32'
                      Init:
                        BinaryExpr op=*
                          Identifier name='r'
                          Identifier name='c'
                    CallExpr
                      Callee:
                        Identifier name='print'
                      Args:
                        Identifier name='product'
        ForStmt cap_kind=0 cap_value='val' cap_index='(null)'
          Iter:
            Identifier name='row'
          Body:
            Block
              ForStmt cap_kind=0 cap_value='k' cap_index='(null)'
                Iter:
                  RangeExpr inclusive=false
                    Start:
                      IntLiteral value=0
                    End:
                      Identifier name='val'
                Body:
                  Block
                    CallExpr
                      Callee:
                        Identifier name='print'
                      Args:
                        Identifier name='k'
  FuncDecl name='process_buffer' realm=stack is_pub=false is_main=false
    Params:
      Param name='buf'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            IntLiteral value=0
        IfStmt
          Condition:
            BinaryExpr op===
              Identifier name='len'
              IntLiteral value=0
          Then:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  UnaryExpr op=-
                    IntLiteral value=1
              ReturnStmt
        ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
          Iter:
            RangeExpr inclusive=false
              Start:
                IntLiteral value=0
              End:
                Identifier name='len'
          Body:
            Block
              VarDecl name='byte' is_const=false is_volatile=false
                TypeExpr kind=named name='u8'
                Init:
                  UnaryExpr op=*
                    BinaryExpr op=+
                      Identifier name='buf'
                      Identifier name='i'
              IfStmt
                Condition:
                  BinaryExpr op===
                    Identifier name='byte'
                    IntLiteral value=0
                Then:
                  Block
                    BreakStmt
              Assign
                Target:
                  Identifier name='r'
                Value:
                  BinaryExpr op=+
                    Identifier name='r'
                    IntLiteral value=1
raul@raul:~$
## test 04: 
raul@raul:~$ ./ast_tool src/tests/samples/04_types_interfaces.runes
[Error] src/tests/samples/04_types_interfaces.runes:136:32: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:176:30: expected ')' after arguments
[Error] src/tests/samples/04_types_interfaces.runes:176:36: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:187:29: expected ')' after arguments
[Error] src/tests/samples/04_types_interfaces.runes:187:51: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:201:32: expected ')' after arguments
[Error] src/tests/samples/04_types_interfaces.runes:201:38: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:202:25: expected ')' after tuple type
[Error] src/tests/samples/04_types_interfaces.runes:202:47: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:212:32: expected ')' after arguments
[Error] src/tests/samples/04_types_interfaces.runes:212:38: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:213:25: expected ')' after expression
[Error] src/tests/samples/04_types_interfaces.runes:213:38: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:215:5: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:220:29: expected a type expression
[Error] src/tests/samples/04_types_interfaces.runes:220:41: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:221:30: expected a type expression
[Error] src/tests/samples/04_types_interfaces.runes:221:46: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:222:29: expected a type expression
[Error] src/tests/samples/04_types_interfaces.runes:222:38: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:224:33: expected a type expression
[Error] src/tests/samples/04_types_interfaces.runes:224:36: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:225:31: expected a type expression
[Error] src/tests/samples/04_types_interfaces.runes:225:41: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:231:38: expected ')' after arguments
[Error] src/tests/samples/04_types_interfaces.runes:231:44: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:231:45: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:236:25: expected ')' after arguments
[Error] src/tests/samples/04_types_interfaces.runes:236:50: expected an expression
[Error] src/tests/samples/04_types_interfaces.runes:236:51: expected an expression
AST for src/tests/samples/04_types_interfaces.runes:
Program
  TypeDecl name='Vec2' is_pub=false
    FieldDecl name='x' is_volatile=false
      TypeExpr kind=named name='f32'
    FieldDecl name='y' is_volatile=false
      TypeExpr kind=named name='f32'
  TypeDecl name='Vec3' is_pub=false
    FieldDecl name='x' is_volatile=false
      TypeExpr kind=named name='f32'
      Default:
        FloatLiteral value=0.000000
    FieldDecl name='y' is_volatile=false
      TypeExpr kind=named name='f32'
      Default:
        FloatLiteral value=0.000000
    FieldDecl name='z' is_volatile=false
      TypeExpr kind=named name='f32'
      Default:
        FloatLiteral value=0.000000
  TypeDecl name='Node' is_pub=false
    FieldDecl name='val' is_volatile=false
      TypeExpr kind=named name='i32'
    FieldDecl name='next' is_volatile=false
      TypeExpr kind=pointer
        TypeExpr kind=named name='Node'
  TypeDecl name='Rect' is_pub=false
    FieldDecl name='x' is_volatile=false
      TypeExpr kind=named name='f32'
    FieldDecl name='y' is_volatile=false
      TypeExpr kind=named name='f32'
    FieldDecl name='w' is_volatile=false
      TypeExpr kind=named name='f32'
    FieldDecl name='h' is_volatile=false
      TypeExpr kind=named name='f32'
  TypeDecl name='Transform' is_pub=false
    FieldDecl name='origin' is_volatile=false
      TypeExpr kind=named name='Vec2'
    FieldDecl name='scale' is_volatile=false
      TypeExpr kind=named name='Vec2'
    FieldDecl name='rotation' is_volatile=false
      TypeExpr kind=named name='f32'
  TypeDecl name='Palette' is_pub=false
    FieldDecl name='name' is_volatile=false
      TypeExpr kind=named name='str'
    FieldDecl name='colors' is_volatile=false
      TypeExpr kind=array
        TypeExpr kind=named name='u32'
        Size:
          IntLiteral value=16
    FieldDecl name='count' is_volatile=false
      TypeExpr kind=named name='u8'
  VariantDecl name='Color' is_pub=false
    VariantArm name='Red'
    VariantArm name='Green'
    VariantArm name='Blue'
    VariantArm name='RGB'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
    VariantArm name='RGBA'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
    VariantArm name='Hex'
      TypeExpr kind=named name='str'
  VariantDecl name='Shape' is_pub=false
    VariantArm name='Circle'
      TypeExpr kind=named name='f32'
    VariantArm name='Rect'
      TypeExpr kind=named name='f32'
      TypeExpr kind=named name='f32'
    VariantArm name='Triangle'
      TypeExpr kind=named name='Vec2'
      TypeExpr kind=named name='Vec2'
      TypeExpr kind=named name='Vec2'
    VariantArm name='Point'
  VariantDecl name='Token' is_pub=false
    VariantArm name='Ident'
      TypeExpr kind=named name='str'
    VariantArm name='Number'
      TypeExpr kind=named name='i64'
    VariantArm name='Float'
      TypeExpr kind=named name='f64'
    VariantArm name='Punct'
      TypeExpr kind=named name='char'
    VariantArm name='Eof'
  VariantDecl name='Result' is_pub=false
    VariantArm name='Ok'
      TypeExpr kind=named name='i32'
    VariantArm name='Err'
      TypeExpr kind=named name='str'
  MethodDecl type_name='Vec2' iface_name='(null)' is_pub=false
    FuncDecl name='length' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='f32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='sqrt'
                Args:
                  BinaryExpr op=+
                    BinaryExpr op=*
                      FieldExpr field='x'
                        Identifier name='self'
                      FieldExpr field='x'
                        Identifier name='self'
                    BinaryExpr op=*
                      FieldExpr field='y'
                        Identifier name='self'
                      FieldExpr field='y'
                        Identifier name='self'
    FuncDecl name='scale' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='factor'
          TypeExpr kind=named name='f32'
      Return name='r'
        TypeExpr kind=named name='Vec2'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='Vec2'
                Args:
                  NamedArg name='x'
                    BinaryExpr op=*
                      FieldExpr field='x'
                        Identifier name='self'
                      Identifier name='factor'
                  NamedArg name='y'
                    BinaryExpr op=*
                      FieldExpr field='y'
                        Identifier name='self'
                      Identifier name='factor'
    FuncDecl name='add' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='other'
          TypeExpr kind=named name='Vec2'
      Return name='r'
        TypeExpr kind=named name='Vec2'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='Vec2'
                Args:
                  NamedArg name='x'
                    BinaryExpr op=+
                      FieldExpr field='x'
                        Identifier name='self'
                      FieldExpr field='x'
                        Identifier name='other'
                  NamedArg name='y'
                    BinaryExpr op=+
                      FieldExpr field='y'
                        Identifier name='self'
                      FieldExpr field='y'
                        Identifier name='other'
    FuncDecl name='dot' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='other'
          TypeExpr kind=named name='Vec2'
      Return name='r'
        TypeExpr kind=named name='f32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op=+
                BinaryExpr op=*
                  FieldExpr field='x'
                    Identifier name='self'
                  FieldExpr field='x'
                    Identifier name='other'
                BinaryExpr op=*
                  FieldExpr field='y'
                    Identifier name='self'
                  FieldExpr field='y'
                    Identifier name='other'
    FuncDecl name='zero' realm=stack is_pub=false is_main=false
      Return name='r'
        TypeExpr kind=named name='Vec2'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='Vec2'
                Args:
                  NamedArg name='x'
                    FloatLiteral value=0.000000
                  NamedArg name='y'
                    FloatLiteral value=0.000000
    FuncDecl name='eq' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='other'
          TypeExpr kind=named name='Vec2'
      Return name='r'
        TypeExpr kind=named name='bool'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op===
                FieldExpr field='x'
                  Identifier name='self'
                FieldExpr field='x'
                  Identifier name='other'
          Identifier name='and'
          BinaryExpr op===
            FieldExpr field='y'
              Identifier name='self'
            FieldExpr field='y'
              Identifier name='other'
  MethodDecl type_name='Vec3' iface_name='(null)' is_pub=false
    FuncDecl name='length' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='f32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='sqrt'
                Args:
                  BinaryExpr op=+
                    BinaryExpr op=+
                      BinaryExpr op=*
                        FieldExpr field='x'
                          Identifier name='self'
                        FieldExpr field='x'
                          Identifier name='self'
                      BinaryExpr op=*
                        FieldExpr field='y'
                          Identifier name='self'
                        FieldExpr field='y'
                          Identifier name='self'
                    BinaryExpr op=*
                      FieldExpr field='z'
                        Identifier name='self'
                      FieldExpr field='z'
                        Identifier name='self'
    FuncDecl name='add' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='other'
          TypeExpr kind=named name='Vec3'
      Return name='r'
        TypeExpr kind=named name='Vec3'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='Vec3'
                Args:
                  NamedArg name='x'
                    BinaryExpr op=+
                      FieldExpr field='x'
                        Identifier name='self'
                      FieldExpr field='x'
                        Identifier name='other'
                  NamedArg name='y'
                    BinaryExpr op=+
                      FieldExpr field='y'
                        Identifier name='self'
                      FieldExpr field='y'
                        Identifier name='other'
                  NamedArg name='z'
                    BinaryExpr op=+
                      FieldExpr field='z'
                        Identifier name='self'
                      FieldExpr field='z'
                        Identifier name='other'
    FuncDecl name='cross' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='other'
          TypeExpr kind=named name='Vec3'
      Return name='r'
        TypeExpr kind=named name='Vec3'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='Vec3'
                Args:
                  NamedArg name='x'
                    BinaryExpr op=-
                      BinaryExpr op=*
                        FieldExpr field='y'
                          Identifier name='self'
                        FieldExpr field='z'
                          Identifier name='other'
                      BinaryExpr op=*
                        FieldExpr field='z'
                          Identifier name='self'
                        FieldExpr field='y'
                          Identifier name='other'
                  NamedArg name='y'
                    BinaryExpr op=-
                      BinaryExpr op=*
                        FieldExpr field='z'
                          Identifier name='self'
                        FieldExpr field='x'
                          Identifier name='other'
                      BinaryExpr op=*
                        FieldExpr field='x'
                          Identifier name='self'
                        FieldExpr field='z'
                          Identifier name='other'
                  NamedArg name='z'
                    BinaryExpr op=-
                      BinaryExpr op=*
                        FieldExpr field='x'
                          Identifier name='self'
                        FieldExpr field='y'
                          Identifier name='other'
                      BinaryExpr op=*
                        FieldExpr field='y'
                          Identifier name='self'
                        FieldExpr field='x'
                          Identifier name='other'
  MethodDecl type_name='Rect' iface_name='(null)' is_pub=false
    FuncDecl name='area' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='f32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op=*
                FieldExpr field='w'
                  Identifier name='self'
                FieldExpr field='h'
                  Identifier name='self'
    FuncDecl name='perimeter' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='f32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op=*
                FloatLiteral value=2.000000
                BinaryExpr op=+
                  FieldExpr field='w'
                    Identifier name='self'
                  FieldExpr field='h'
                    Identifier name='self'
    FuncDecl name='contains' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='p'
          TypeExpr kind=named name='Vec2'
      Return name='r'
        TypeExpr kind=named name='bool'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op=>=
                FieldExpr field='x'
                  Identifier name='p'
                FieldExpr field='x'
                  Identifier name='self'
          VarDecl name='p' is_const=false is_volatile=false
            TypeExpr kind=named name='and'
  MethodDecl type_name='Node' iface_name='(null)' is_pub=false
    FuncDecl name='new' realm=dynamic is_pub=false is_main=false
      Params:
        Param name='val'
          TypeExpr kind=named name='i32'
      Return name='r'
        TypeExpr kind=pointer
          TypeExpr kind=named name='Node'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='raw_alloc'
                Args:
                  CallExpr
                    Callee:
                      Identifier name='sizeof'
                    Args:
                      Identifier name='Node'
          Assign
            Target:
              FieldExpr field='val'
                Identifier name='r'
            Value:
              Identifier name='val'
          Assign
            Target:
              FieldExpr field='next'
                Identifier name='r'
            Value:
              CastExpr kind=0
                Expr:
                  IntLiteral value=0
                Target Type:
                  TypeExpr kind=pointer
                    TypeExpr kind=named name='Node'
    FuncDecl name='is_tail' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='bool'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op===
                FieldExpr field='next'
                  Identifier name='self'
                CastExpr kind=0
                  Expr:
                    IntLiteral value=0
                  Target Type:
                    TypeExpr kind=pointer
                      TypeExpr kind=named name='Node'
  InterfaceDecl name='Drawable' is_pub=false
    FuncDecl name='draw' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
    FuncDecl name='bbox' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=tuple
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
  InterfaceDecl name='Hashable' is_pub=false
    FuncDecl name='hash' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='u64'
    FuncDecl name='eq' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='other'
          TypeExpr kind=named name='Hashable'
      Return name='r'
        TypeExpr kind=named name='bool'
  InterfaceDecl name='Serializable' is_pub=false
    FuncDecl name='to_bytes' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
    FuncDecl name='byte_len' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='usize'
  MethodDecl type_name='Vec2' iface_name='Drawable' is_pub=false
    FuncDecl name='draw' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Body:
        Block
    FuncDecl name='bbox' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=tuple
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              TupleExpr
                FieldExpr field='x'
                  Identifier name='self'
                FieldExpr field='y'
                  Identifier name='self'
                FieldExpr field='x'
                  Identifier name='self'
                FieldExpr field='y'
                  Identifier name='self'
  MethodDecl type_name='Rect' iface_name='Drawable' is_pub=false
    FuncDecl name='draw' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Body:
        Block
    FuncDecl name='bbox' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=tuple
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
          TypeExpr kind=named name='f32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              TupleExpr
                FieldExpr field='x'
                  Identifier name='self'
                FieldExpr field='y'
                  Identifier name='self'
                BinaryExpr op=+
                  FieldExpr field='x'
                    Identifier name='self'
                  FieldExpr field='w'
                    Identifier name='self'
                BinaryExpr op=+
                  FieldExpr field='y'
                    Identifier name='self'
                  FieldExpr field='h'
                    Identifier name='self'
  FuncDecl name='demo_instantiation' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='Vec3' is_const=false is_volatile=false
          TypeExpr kind=tuple
        Assign
          Target:
            Identifier name='up'
          Value:
            CallExpr
              Callee:
                Identifier name='Vec3'
              Args:
                NamedArg name='y'
                  FloatLiteral value=1.000000
        VarDecl name='len' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            CallExpr
              Callee:
                FieldExpr field='length'
                  Identifier name='v'
        VarDecl name='scaled' is_const=false is_volatile=false
          TypeExpr kind=named name='Vec2'
          Init:
            CallExpr
              Callee:
                FieldExpr field='scale'
                  Identifier name='v'
              Args:
                FloatLiteral value=2.000000
        VarDecl name='d' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            CallExpr
              Callee:
                FieldExpr field='dot'
                  Identifier name='v'
              Args:
                Identifier name='sum'
        CallExpr
          Callee:
            Identifier name='render'
          Args:
            Identifier name='v'
  FuncDecl name='render' realm=stack is_pub=false is_main=false
    Params:
      Param name='d'
        TypeExpr kind=named name='Drawable'
    Body:
      Block
        VarDecl name='box' is_const=false is_volatile=false
          TypeExpr kind=tuple
            TypeExpr kind=named name='f32'
            TypeExpr kind=named name='f32'
            TypeExpr kind=named name='f32'
            TypeExpr kind=named name='f32'
          Init:
            CallExpr
              Callee:
                FieldExpr field='bbox'
                  Identifier name='d'
        CallExpr
          Callee:
            FieldExpr field='draw'
              Identifier name='d'
raul@raul:~$
## test 05: 
raul@raul:~$ ./ast_tool src/tests/samples/05_error_handling.runes
[Error] src/tests/samples/05_error_handling.runes:145:9: expected an expression
[Error] src/tests/samples/05_error_handling.runes:145:26: expected an expression
[Error] src/tests/samples/05_error_handling.runes:146:16: expected an expression
[Error] src/tests/samples/05_error_handling.runes:149:10: expected an expression
[Error] src/tests/samples/05_error_handling.runes:150:16: expected an expression
[Error] src/tests/samples/05_error_handling.runes:153:9: expected an expression
[Error] src/tests/samples/05_error_handling.runes:154:5: expected an expression
[Error] src/tests/samples/05_error_handling.runes:155:1: expected an expression
[Error] src/tests/samples/05_error_handling.runes:190:38: expected ')' after arguments
[Error] src/tests/samples/05_error_handling.runes:190:46: expected an expression
[Error] src/tests/samples/05_error_handling.runes:199:19: expected ')' after arguments
[Error] src/tests/samples/05_error_handling.runes:199:29: expected an expression
[Error] src/tests/samples/05_error_handling.runes:206:1: expected an expression
AST for src/tests/samples/05_error_handling.runes:
Program
  ErrorDecl name='MathError' is_pub=false
    Identifier name='DivByZero'
    Identifier name='Overflow'
    Identifier name='Underflow'
    Identifier name='NaN'
  ErrorDecl name='PageFault' is_pub=false
    Identifier name='NotMapped'
    Identifier name='PermissionDenied'
    Identifier name='OutOfMemory'
    Identifier name='InvalidAddress'
  ErrorDecl name='ParseError' is_pub=false
    Identifier name='UnexpectedToken'
    Identifier name='UnexpectedEOF'
    Identifier name='InvalidLiteral'
    Identifier name='InvalidEscape'
  ErrorDecl name='IoError' is_pub=false
    Identifier name='NotFound'
    Identifier name='PermissionDenied'
    Identifier name='BrokenPipe'
    Identifier name='TimedOut'
  ErrorDecl name='MapError' is_pub=false
    Identifier name='AlreadyMapped'
    Identifier name='InvalidAddress'
    Identifier name='OutOfMemory'
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
  FuncDecl name='safe_sqrt' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='f32'
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='f32'
    Body:
      Block
        IfStmt
          Condition:
            BinaryExpr op=<
              Identifier name='x'
              FloatLiteral value=0.000000
          Then:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  ErrorExpr
                    Identifier name='MathError'
                    Identifier name='NaN'
          Else:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  CallExpr
                    Callee:
                      Identifier name='sqrt'
                    Args:
                      Identifier name='x'
  FuncDecl name='alloc_page' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
    Body:
      Block
        VarDecl name='p' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CallExpr
              Callee:
                Identifier name='raw_alloc'
              Args:
                IntLiteral value=4096
        IfStmt
          Condition:
            BinaryExpr op===
              Identifier name='p'
              CastExpr kind=0
                Expr:
                  IntLiteral value=0
                Target Type:
                  TypeExpr kind=pointer
                    TypeExpr kind=named name='u8'
          Then:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  ErrorExpr
                    Identifier name='PageFault'
                    Identifier name='OutOfMemory'
          Else:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  Identifier name='p'
  FuncDecl name='map_region' realm=stack is_pub=false is_main=false
    Params:
      Param name='start'
        TypeExpr kind=named name='u64'
      Param name='end'
        TypeExpr kind=named name='u64'
      Param name='flags'
        TypeExpr kind=named name='u64'
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='void'
    Body:
      Block
        IfStmt
          Condition:
            BinaryExpr op=<=
              Identifier name='end'
              Identifier name='start'
          Then:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  ErrorExpr
                    Identifier name='MapError'
                    Identifier name='InvalidAddress'
              ReturnStmt
        VarDecl name='addr' is_const=false is_volatile=false
          TypeExpr kind=named name='u64'
          Init:
            Identifier name='start'
        WhileStmt
          Condition:
            BinaryExpr op=<
              Identifier name='addr'
              Identifier name='end'
          Body:
            Block
              TryExpr
                CallExpr
                  Callee:
                    Identifier name='map_page'
                  Args:
                    Identifier name='addr'
                    Identifier name='addr'
                    Identifier name='flags'
              Assign
                Target:
                  Identifier name='addr'
                Value:
                  BinaryExpr op=+
                    Identifier name='addr'
                    IntLiteral value=4096
  FuncDecl name='run_math' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='f32'
    Body:
      Block
        VarDecl name='a' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='divide'
                Args:
                  FloatLiteral value=10.000000
                  FloatLiteral value=2.000000
        VarDecl name='b' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='safe_sqrt'
                Args:
                  Identifier name='a'
        Assign
          Target:
            Identifier name='r'
          Value:
            BinaryExpr op=*
              Identifier name='b'
              FloatLiteral value=2.000000
  FuncDecl name='setup_vm' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='void'
    Body:
      Block
        VarDecl name='pml4_page' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='alloc_page'
        VarDecl name='pdpt_page' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='alloc_page'
        TryExpr
          CallExpr
            Callee:
              Identifier name='map_region'
            Args:
              IntLiteral value=18446603336221196288
              IntLiteral value=18446603336223293440
              IntLiteral value=3
  FuncDecl name='safe_divide_block' realm=stack is_pub=false is_main=false
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
                      StringLiteral value="caught error:"
                      Identifier name='e'
                  ReturnStmt
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            Identifier name='val'
  FuncDecl name='safe_divide_with_log' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='result' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            CatchExpr err_name='e'
              Expr:
                CallExpr
                  Callee:
                    Identifier name='divide'
                  Args:
                    FloatLiteral value=100.000000
                    FloatLiteral value=0.000000
              Handler:
                Block
                  CallExpr
                    Callee:
                      Identifier name='log_error'
                    Args:
                      Identifier name='e'
                  Assign
                    Target:
                      Identifier name='result'
                    Value:
                      UnaryExpr op=-
                        FloatLiteral value=1.000000
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            StringLiteral value="result:"
            Identifier name='result'
  FuncDecl name='safe_divide_default' realm=stack is_pub=false is_main=false
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
        VarDecl name='root' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            CatchExpr err_name='(null)'
              Expr:
                CallExpr
                  Callee:
                    Identifier name='safe_sqrt'
                  Args:
                    UnaryExpr op=-
                      FloatLiteral value=4.000000
              Handler:
                UnaryExpr op=-
                  FloatLiteral value=1.000000
        CallExpr
          Callee:
            Identifier name='print'
          Args:
            Identifier name='root'
  FuncDecl name='match_divide' realm=stack is_pub=false is_main=false
    Body:
      Block
        MatchStmt
          Subject:
            CallExpr
              Callee:
                Identifier name='divide'
              Args:
                FloatLiteral value=10.000000
                FloatLiteral value=0.000000
          Arms:
            MatchArm
              Pattern:
                CallExpr
                  Callee:
                    Identifier name='Ok'
                  Args:
                    Identifier name='v'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="result:"
                    Identifier name='v'
            MatchArm
              Pattern:
                CallExpr
                  Callee:
                    Identifier name='Err'
                  Args:
                    Identifier name='e'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="error:"
                    Identifier name='e'
  FuncDecl name='match_alloc' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        Identifier name='p'
        TupleExpr
          Identifier name='p'
          IntLiteral value=0
          IntLiteral value=4096
        IntLiteral value=0
  Identifier name='e'
  Identifier name='e'
  UnaryExpr op=-
    IntLiteral value=1
  FuncDecl name='parse_and_eval' realm=stack is_pub=false is_main=false
    Params:
      Param name='src'
        TypeExpr kind=named name='str'
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='i32'
    Body:
      Block
        VarDecl name='ast' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='Node'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='parse'
                Args:
                  Identifier name='src'
        VarDecl name='result' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='eval'
                Args:
                  Identifier name='ast'
        Assign
          Target:
            Identifier name='r'
          Value:
            Identifier name='result'
  FuncDecl name='parse_file' realm=stack is_pub=false is_main=false
    Params:
      Param name='path'
        TypeExpr kind=named name='str'
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=pointer
          TypeExpr kind=named name='Node'
    Body:
      Block
        VarDecl name='source' is_const=false is_volatile=false
          TypeExpr kind=named name='str'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='read_file'
                Args:
                  Identifier name='path'
        VarDecl name='ast' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='Node'
          Init:
            TryExpr
              CallExpr
                Callee:
                  Identifier name='parse'
                Args:
                  Identifier name='source'
        Assign
          Target:
            Identifier name='r'
          Value:
            Identifier name='ast'
  MethodDecl type_name='PageTable' iface_name='(null)' is_pub=false
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
  FuncDecl name='setup_paging' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='void'
    Body:
      Block
        VarDecl name='pml4' is_const=false is_volatile=false
          TypeExpr kind=named name='PageTable'
          Init:
            CallExpr
              Callee:
                FieldExpr field='new'
                  Identifier name='PageTable'
        TupleExpr
          IntLiteral value=18446603336221200384
          IntLiteral value=4096
          IntLiteral value=3
  FuncDecl name='map_or_panic' realm=stack is_pub=false is_main=false
    Params:
      Param name='pt'
        TypeExpr kind=named name='PageTable'
      Param name='vaddr'
        TypeExpr kind=named name='u64'
      Param name='paddr'
        TypeExpr kind=named name='u64'
    Body:
      Block
        MatchStmt
          Subject:
            Identifier name='e'
          Arms:
            MatchArm
              Pattern:
                FieldExpr field='AlreadyMapped'
                  Identifier name='MapError'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="already mapped, skipping"
            MatchArm
              Pattern:
                FieldExpr field='InvalidAddress'
                  Identifier name='MapError'
              Body:
                CallExpr
                  Callee:
                    Identifier name='panic'
                  Args:
                    StringLiteral value="invalid address"
            MatchArm
              Pattern:
                FieldExpr field='OutOfMemory'
                  Identifier name='MapError'
              Body:
                CallExpr
                  Callee:
                    Identifier name='panic'
                  Args:
                    StringLiteral value="OOM"
raul@raul:~$
## test 06: 
raul@raul:~$ ./ast_tool src/tests/samples/06_pattern_matching.runes
[Error] src/tests/samples/06_pattern_matching.runes:57:15: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:57:16: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:57:48: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:57:49: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:58:46: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:58:48: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:59:44: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:68:9: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:74:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:77:9: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:78:31: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:78:59: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:79:31: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:79:39: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:80:31: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:84:10: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:86:5: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:87:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:90:9: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:91:19: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:91:34: expected variable name
[Error] src/tests/samples/06_pattern_matching.runes:92:19: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:93:19: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:94:19: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:97:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:105:46: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:105:47: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:106:40: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:107:24: expected '{'
[Error] src/tests/samples/06_pattern_matching.runes:107:45: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:108:24: expected '{'
[Error] src/tests/samples/06_pattern_matching.runes:108:46: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:109:24: expected '{'
[Error] src/tests/samples/06_pattern_matching.runes:109:45: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:110:42: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:115:9: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:116:25: expected '{'
[Error] src/tests/samples/06_pattern_matching.runes:117:25: expected '{'
[Error] src/tests/samples/06_pattern_matching.runes:118:25: expected '{'
[Error] src/tests/samples/06_pattern_matching.runes:119:25: expected '{'
[Error] src/tests/samples/06_pattern_matching.runes:122:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:130:22: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:130:28: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:130:48: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:131:15: expected ')' after expression
[Error] src/tests/samples/06_pattern_matching.runes:131:23: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:131:56: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:132:18: expected ')' after tuple
[Error] src/tests/samples/06_pattern_matching.runes:132:23: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:132:56: expected '->' after match pattern
[Error] src/tests/samples/06_pattern_matching.runes:133:49: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:133:53: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:143:22: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:144:13: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:144:28: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:149:32: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:149:53: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:150:10: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:151:20: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:151:42: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:152:43: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:153:5: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:154:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:161:9: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:162:26: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:165:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:168:9: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:169:18: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:172:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:187:24: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:187:27: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:188:16: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:188:34: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:189:16: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:189:35: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:191:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:194:9: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:194:33: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:195:16: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:196:16: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:198:1: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:203:27: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:203:34: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:204:24: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:206:37: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:206:43: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:208:37: expected ')' after arguments
[Error] src/tests/samples/06_pattern_matching.runes:208:43: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:210:14: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:211:38: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:211:67: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:212:38: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:212:64: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:214:5: expected an expression
[Error] src/tests/samples/06_pattern_matching.runes:215:1: expected an expression
AST for src/tests/samples/06_pattern_matching.runes:
Program
  VariantDecl name='Color' is_pub=false
    VariantArm name='Red'
    VariantArm name='Green'
    VariantArm name='Blue'
    VariantArm name='RGB'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
    VariantArm name='RGBA'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
      TypeExpr kind=named name='u8'
    VariantArm name='Hex'
      TypeExpr kind=named name='str'
  VariantDecl name='Shape' is_pub=false
    VariantArm name='Circle'
      TypeExpr kind=named name='f32'
    VariantArm name='Rect'
      TypeExpr kind=named name='f32'
      TypeExpr kind=named name='f32'
    VariantArm name='Triangle'
      TypeExpr kind=named name='f32'
      TypeExpr kind=named name='f32'
      TypeExpr kind=named name='f32'
    VariantArm name='Point'
  VariantDecl name='Token' is_pub=false
    VariantArm name='Ident'
      TypeExpr kind=named name='str'
    VariantArm name='Number'
      TypeExpr kind=named name='i64'
    VariantArm name='Float'
      TypeExpr kind=named name='f64'
    VariantArm name='Punct'
      TypeExpr kind=named name='char'
    VariantArm name='Eof'
  TypeDecl name='Vec2' is_pub=false
    FieldDecl name='x' is_volatile=false
      TypeExpr kind=named name='f32'
    FieldDecl name='y' is_volatile=false
      TypeExpr kind=named name='f32'
  ErrorDecl name='MathError' is_pub=false
    Identifier name='DivByZero'
    Identifier name='Overflow'
  FuncDecl name='describe_color' realm=stack is_pub=false is_main=false
    Params:
      Param name='c'
        TypeExpr kind=named name='Color'
    Body:
      Block
        MatchStmt
          Subject:
            Identifier name='c'
          Arms:
            MatchArm
              Pattern:
                Identifier name='Red'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="red"
            MatchArm
              Pattern:
                Identifier name='Green'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="green"
            MatchArm
              Pattern:
                Identifier name='Blue'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="blue"
            MatchArm
              Pattern:
                CallExpr
                  Callee:
                    Identifier name='RGB'
                  Args:
                    Identifier name='r'
                    Identifier name='g'
                    Identifier name='b'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="rgb:"
                    Identifier name='r'
                    Identifier name='g'
                    Identifier name='b'
            MatchArm
              Pattern:
                CallExpr
                  Callee:
                    Identifier name='RGBA'
                  Args:
                    Identifier name='r'
                    Identifier name='g'
                    Identifier name='b'
                    Identifier name='a'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="rgba:"
                    Identifier name='r'
                    Identifier name='g'
                    Identifier name='b'
                    Identifier name='a'
            MatchArm
              Pattern:
                CallExpr
                  Callee:
                    Identifier name='Hex'
                  Args:
                    Identifier name='s'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="hex:"
                    Identifier name='s'
  FuncDecl name='describe_token' realm=stack is_pub=false is_main=false
    Params:
      Param name='t'
        TypeExpr kind=named name='Token'
    Body:
      Block
        MatchStmt
          Subject:
            Identifier name='t'
          Arms:
            MatchArm
              Pattern:
                CallExpr
                  Callee:
                    Identifier name='Ident'
                  Args:
                    Identifier name='name'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="identifier:"
                    Identifier name='name'
            MatchArm
              Pattern:
                CallExpr
                  Callee:
                    Identifier name='Number'
                  Args:
                    Identifier name='n'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="int literal:"
                    Identifier name='n'
  FuncDecl name='color_name' realm=stack is_pub=false is_main=false
    Params:
      Param name='c'
        TypeExpr kind=named name='Color'
    Return name='r'
      TypeExpr kind=named name='str'
    Body:
      Block
  FuncDecl name='shape_area' realm=stack is_pub=false is_main=false
    Params:
      Param name='s'
        TypeExpr kind=named name='Shape'
    Return name='r'
      TypeExpr kind=named name='f32'
    Body:
      Block
        Identifier name='radius'
        BinaryExpr op=*
          UnaryExpr op=*
            Identifier name='radius'
          Identifier name='radius'
        TupleExpr
          Identifier name='w'
          Identifier name='h'
        UnaryExpr op=*
          Identifier name='h'
        TupleExpr
          Identifier name='a'
          Identifier name='b'
          Identifier name='c'
        VarDecl name='sp' is_const=false is_volatile=false
          TypeExpr kind=named name='f32'
          Init:
            BinaryExpr op=/
              BinaryExpr op=+
                BinaryExpr op=+
                  Identifier name='a'
                  Identifier name='b'
                Identifier name='c'
              FloatLiteral value=2.000000
        CallExpr
          Callee:
            Identifier name='sqrt'
          Args:
            BinaryExpr op=*
              BinaryExpr op=*
                BinaryExpr op=*
                  Identifier name='sp'
                  BinaryExpr op=-
                    Identifier name='sp'
                    Identifier name='a'
                BinaryExpr op=-
                  Identifier name='sp'
                  Identifier name='b'
              BinaryExpr op=-
                Identifier name='sp'
                Identifier name='c'
  FuncDecl name='token_len' realm=stack is_pub=false is_main=false
    Params:
      Param name='t'
        TypeExpr kind=named name='Token'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        Identifier name='s'
        VarDecl name='(null)' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
        Identifier name='_'
        Identifier name='_'
        Identifier name='_'
  FuncDecl name='classify_int' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
    Body:
      Block
        MatchStmt
          Subject:
            Identifier name='x'
          Arms:
  FuncDecl name='classify_float' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='f32'
    Return name='r'
      TypeExpr kind=named name='str'
    Body:
      Block
  FuncDecl name='classify_point' realm=stack is_pub=false is_main=false
    Params:
      Param name='p'
        TypeExpr kind=named name='Vec2'
    Body:
      Block
        MatchStmt
          Subject:
            Identifier name='p'
          Arms:
  FuncDecl name='describe_shape_color' realm=stack is_pub=false is_main=false
    Params:
      Param name='s'
        TypeExpr kind=named name='Shape'
      Param name='c'
        TypeExpr kind=named name='Color'
    Body:
      Block
        MatchStmt
          Subject:
            Identifier name='s'
          Arms:
  TupleExpr
    Identifier name='w'
    Identifier name='h'
  TupleExpr
    StringLiteral value="rect"
    Identifier name='w'
    Identifier name='h'
  StringLiteral value="other shape"
  FuncDecl name='has_alpha' realm=stack is_pub=false is_main=false
    Params:
      Param name='c'
        TypeExpr kind=named name='Color'
    Return name='r'
      TypeExpr kind=named name='bool'
    Body:
      Block
        TupleExpr
          Identifier name='_'
          Identifier name='_'
          Identifier name='_'
          Identifier name='a'
  FuncDecl name='is_punctuation' realm=stack is_pub=false is_main=false
    Params:
      Param name='t'
        TypeExpr kind=named name='Token'
    Return name='r'
      TypeExpr kind=named name='bool'
    Body:
      Block
        Identifier name='_'
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
  FuncDecl name='match_result' realm=stack is_pub=false is_main=false
    Body:
      Block
        Identifier name='v'
        TupleExpr
          StringLiteral value="ok:"
          Identifier name='v'
        Identifier name='e'
        TupleExpr
          StringLiteral value="err:"
          Identifier name='e'
  FuncDecl name='match_result_expr' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=named name='f32'
    Body:
      Block
        TupleExpr
          FloatLiteral value=10.000000
          FloatLiteral value=0.000000
        Identifier name='v'
        Identifier name='_'
  FuncDecl name='process_values' realm=stack is_pub=false is_main=false
    Params:
      Param name='buf'
        TypeExpr kind=array
          TypeExpr kind=named name='f32'
          Size:
            IntLiteral value=8
      Param name='divisor'
        TypeExpr kind=named name='f32'
    Body:
      Block
        ForStmt cap_kind=0 cap_value='val' cap_index='(null)'
          Iter:
            Identifier name='buf'
          Body:
            Block
              Identifier name='result'
              IfStmt
                Condition:
                  BinaryExpr op=>
                    Identifier name='result'
                    FloatLiteral value=1.000000
                Then:
                  Block
                Else:
                  Block
        FieldExpr field='DivByZero'
          Identifier name='MathError'
        StringLiteral value="skipping zero div"
        Identifier name='e'
        TupleExpr
          StringLiteral value="unexpected:"
          Identifier name='e'
raul@raul:~$
## test 07:  
raul@raul:~$ ./ast_tool src/tests/samples/07_unsafe_systems.runes
[Error] src/tests/samples/07_unsafe_systems.runes:82:1: expected a declaration
[Error] src/tests/samples/07_unsafe_systems.runes:85:1: expected a declaration
[Error] src/tests/samples/07_unsafe_systems.runes:89:1: expected a declaration
[Error] src/tests/samples/07_unsafe_systems.runes:89:7: expected an expression
[Error] src/tests/samples/07_unsafe_systems.runes:93:1: expected a declaration
[Error] src/tests/samples/07_unsafe_systems.runes:93:9: expected an expression
[Error] src/tests/samples/07_unsafe_systems.runes:203:22: expected ')' after arguments
[Error] src/tests/samples/07_unsafe_systems.runes:203:29: expected an expression
[Error] src/tests/samples/07_unsafe_systems.runes:263:30: expected ')' after arguments
[Error] src/tests/samples/07_unsafe_systems.runes:263:36: expected an expression
[Error] src/tests/samples/07_unsafe_systems.runes:265:17: expected an expression
AST for src/tests/samples/07_unsafe_systems.runes:
Program
  UseDecl
    Identifier name='kernel'
    Identifier name='arch'
    Identifier name='x86'
  ExternDecl name='memset' is_func=true
    Params:
      Param name='ptr'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='val'
        TypeExpr kind=named name='i32'
      Param name='len'
        TypeExpr kind=named name='usize'
  ExternDecl name='memcpy' is_func=true
    Params:
      Param name='dst'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='src'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
  ExternDecl name='memcmp' is_func=true
    Params:
      Param name='a'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='b'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
    Return name='r'
      TypeExpr kind=named name='i32'
  ExternDecl name='memmove' is_func=true
    Params:
      Param name='dst'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='src'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
  ExternDecl name='KERNEL_START' is_func=false
    TypeExpr kind=named name='u64'
  ExternDecl name='KERNEL_END' is_func=false
    TypeExpr kind=named name='u64'
  ExternDecl name='BSS_START' is_func=false
    TypeExpr kind=named name='u64'
  ExternDecl name='BSS_END' is_func=false
    TypeExpr kind=named name='u64'
  ExternDecl name='STACK_TOP' is_func=false
    TypeExpr kind=named name='u64'
  ExternDecl name='STACK_BOTTOM' is_func=false
    TypeExpr kind=named name='u64'
  TypeDecl name='PageTable' is_pub=false
    FieldDecl name='entries' is_volatile=false
      TypeExpr kind=array
        TypeExpr kind=named name='u64'
        Size:
          IntLiteral value=512
  TypeDecl name='SyscallFrame' is_pub=false
    FieldDecl name='rax' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rbx' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rcx' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rdx' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rsi' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rdi' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rbp' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='r8' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='r9' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='r10' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='r11' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='r12' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='r13' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='r14' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='r15' is_volatile=false
      TypeExpr kind=named name='u64'
  TypeDecl name='UARTRegs' is_pub=false
    FieldDecl name='data' is_volatile=true
      TypeExpr kind=named name='u8'
    FieldDecl name='status' is_volatile=true
      TypeExpr kind=named name='u8'
    FieldDecl name='control' is_volatile=true
      TypeExpr kind=named name='u16'
    FieldDecl name='baud' is_volatile=true
      TypeExpr kind=named name='u32'
  TypeDecl name='InterruptFrame' is_pub=false
    FieldDecl name='rip' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='cs' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rflags' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rsp' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='ss' is_volatile=false
      TypeExpr kind=named name='u64'
  VarDecl name='IDT_TABLE' is_const=false is_volatile=false
    TypeExpr kind=array
      TypeExpr kind=named name='u64'
      Size:
        IntLiteral value=256
    Init:
      ArrayLiteral
  VarDecl name='GDT_TABLE' is_const=false is_volatile=false
    TypeExpr kind=array
      TypeExpr kind=named name='u64'
      Size:
        IntLiteral value=8
    Init:
      ArrayLiteral
  VarDecl name='KERNEL_STACK' is_const=false is_volatile=false
    TypeExpr kind=named name='u8'
    Init:
      ArrayLiteral
  VarDecl name='KERNEL_HEAP' is_const=false is_volatile=false
    TypeExpr kind=named name='u8'
    Init:
      ArrayLiteral
  FuncDecl name='uart_write' realm=stack is_pub=false is_main=false
    Params:
      Param name='ch'
        TypeExpr kind=named name='u8'
    Body:
      Block
        VarDecl name='uart' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u32'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=268435456
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u32'
        Assign
          Target:
            UnaryExpr op=*
              Identifier name='uart'
          Value:
            CastExpr kind=0
              Expr:
                Identifier name='ch'
              Target Type:
                TypeExpr kind=named name='u32'
  FuncDecl name='uart_read' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=named name='u8'
    Body:
      Block
        VarDecl name='uart' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u32'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=268435456
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u32'
        Assign
          Target:
            Identifier name='r'
          Value:
            CastExpr kind=0
              Expr:
                BinaryExpr op=&
                  UnaryExpr op=*
                    Identifier name='uart'
                  IntLiteral value=255
              Target Type:
                TypeExpr kind=named name='u8'
  FuncDecl name='uart_status' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=named name='u8'
    Body:
      Block
        VarDecl name='uart' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u32'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=268435457
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u32'
        Assign
          Target:
            Identifier name='r'
          Value:
            CastExpr kind=0
              Expr:
                BinaryExpr op=&
                  UnaryExpr op=*
                    Identifier name='uart'
                  IntLiteral value=1
              Target Type:
                TypeExpr kind=named name='u8'
  FuncDecl name='pic_eoi' realm=stack is_pub=false is_main=false
    Params:
      Param name='irq'
        TypeExpr kind=named name='u8'
    Body:
      Block
        VarDecl name='pic1' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=32
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u8'
        VarDecl name='pic2' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=160
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u8'
        IfStmt
          Condition:
            BinaryExpr op=>=
              Identifier name='irq'
              IntLiteral value=8
          Then:
            Block
              Assign
                Target:
                  UnaryExpr op=*
                    Identifier name='pic2'
                Value:
                  IntLiteral value=32
        Assign
          Target:
            UnaryExpr op=*
              Identifier name='pic1'
          Value:
            IntLiteral value=32
  FuncDecl name='zero_page' realm=stack is_pub=false is_main=false
    Params:
      Param name='addr'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
    Body:
      Block
        UnsafeBlock
          Body:
            Block
              ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
                Iter:
                  RangeExpr inclusive=false
                    Start:
                      IntLiteral value=0
                    End:
                      Identifier name='len'
                Body:
                  Block
                    Assign
                      Target:
                        UnaryExpr op=*
                          BinaryExpr op=+
                            Identifier name='addr'
                            Identifier name='i'
                      Value:
                        IntLiteral value=0
  FuncDecl name='copy_bytes' realm=stack is_pub=false is_main=false
    Params:
      Param name='dst'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='src'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
    Body:
      Block
        UnsafeBlock
          Body:
            Block
              ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
                Iter:
                  RangeExpr inclusive=false
                    Start:
                      IntLiteral value=0
                    End:
                      Identifier name='len'
                Body:
                  Block
                    Assign
                      Target:
                        UnaryExpr op=*
                          BinaryExpr op=+
                            Identifier name='dst'
                            Identifier name='i'
                      Value:
                        UnaryExpr op=*
                          BinaryExpr op=+
                            Identifier name='src'
                            Identifier name='i'
  FuncDecl name='read_phys' realm=stack is_pub=false is_main=false
    Params:
      Param name='addr'
        TypeExpr kind=named name='u64'
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        UnsafeBlock
          Body:
            Block
              VarDecl name='ptr' is_const=false is_volatile=false
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u64'
                Init:
                  CastExpr kind=0
                    Expr:
                      Identifier name='addr'
                    Target Type:
                      TypeExpr kind=pointer
                        TypeExpr kind=named name='u64'
              Assign
                Target:
                  Identifier name='r'
                Value:
                  UnaryExpr op=*
                    Identifier name='ptr'
  FuncDecl name='write_phys' realm=stack is_pub=false is_main=false
    Params:
      Param name='addr'
        TypeExpr kind=named name='u64'
      Param name='val'
        TypeExpr kind=named name='u64'
    Body:
      Block
        UnsafeBlock
          Body:
            Block
              VarDecl name='ptr' is_const=false is_volatile=false
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u64'
                Init:
                  CastExpr kind=0
                    Expr:
                      Identifier name='addr'
                    Target Type:
                      TypeExpr kind=pointer
                        TypeExpr kind=named name='u64'
              Assign
                Target:
                  UnaryExpr op=*
                    Identifier name='ptr'
                Value:
                  Identifier name='val'
  FuncDecl name='halt' realm=stack is_pub=false is_main=false
    Body:
      Block
        AsmExpr code='cli; hlt' output='(none)'
  FuncDecl name='cli' realm=stack is_pub=false is_main=false
    Body:
      Block
        AsmExpr code='cli' output='(none)'
  FuncDecl name='sti' realm=stack is_pub=false is_main=false
    Body:
      Block
        AsmExpr code='sti' output='(none)'
  FuncDecl name='read_cr3' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        AsmExpr code='mov %cr3, %rax' output='r'
  FuncDecl name='write_cr3' realm=stack is_pub=false is_main=false
    Params:
      Param name='val'
        TypeExpr kind=named name='u64'
    Body:
      Block
        AsmExpr code='mov %rax, %cr3' output='(none)'
  FuncDecl name='read_cr2' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        AsmExpr code='mov %cr2, %rax' output='r'
  FuncDecl name='cpuid' realm=stack is_pub=false is_main=false
    Params:
      Param name='leaf'
        TypeExpr kind=named name='u32'
    Return name='r'
      TypeExpr kind=tuple
        TypeExpr kind=named name='u32'
        TypeExpr kind=named name='u32'
        TypeExpr kind=named name='u32'
        TypeExpr kind=named name='u32'
    Body:
      Block
        AsmExpr code='cpuid' output='r'
  FuncDecl name='enable_sse' realm=stack is_pub=false is_main=false
    Body:
      Block
        UnsafeBlock
          Body:
            Block
              AsmExpr code='mov %cr0, %rax; and $0xFFFB, %ax; or $0x2, %ax; mov %rax, %cr0' output='(none)'
              AsmExpr code='mov %cr4, %rax; or $0x600, %rax; mov %rax, %cr4' output='(none)'
  FuncDecl name='syscall_entry' realm=stack is_pub=false is_main=false
    Params:
      Param name='nr'
        TypeExpr kind=named name='u64'
      Param name='a'
        TypeExpr kind=named name='u64'
      Param name='b'
        TypeExpr kind=named name='u64'
      Param name='c'
        TypeExpr kind=named name='u64'
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
  FuncDecl name='efi_main' realm=stack is_pub=false is_main=false
    Params:
      Param name='handle'
        TypeExpr kind=pointer
          TypeExpr kind=named name='void'
      Param name='table'
        TypeExpr kind=pointer
          TypeExpr kind=named name='EFISystemTable'
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='init_output'
          Args:
            Identifier name='table'
        CallExpr
          Callee:
            Identifier name='kernel_main'
        Assign
          Target:
            Identifier name='r'
          Value:
            IntLiteral value=0
  FuncDecl name='divide_by_zero_handler' realm=stack is_pub=false is_main=false
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='uart_write'
          Args:
            IntLiteral value=48
        LoopStmt
          Body:
            Block
              UnsafeBlock
                Body:
                  Block
                    AsmExpr code='cli; hlt' output='(none)'
  FuncDecl name='page_fault_handler' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='cr2' is_const=false is_volatile=false
          TypeExpr kind=named name='u64'
          Init:
            CallExpr
              Callee:
                Identifier name='read_cr2'
        CallExpr
          Callee:
            Identifier name='handle_page_fault'
          Args:
            Identifier name='cr2'
  FuncDecl name='double_fault_handler' realm=stack is_pub=false is_main=false
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='uart_write'
          Args:
            IntLiteral value=33
        LoopStmt
          Body:
            Block
              UnsafeBlock
                Body:
                  Block
                    AsmExpr code='cli; hlt' output='(none)'
  FuncDecl name='general_protection_fault' realm=stack is_pub=false is_main=false
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='panic'
          Args:
            StringLiteral value="GPF"
  FuncDecl name='timer_handler' realm=stack is_pub=false is_main=false
    Body:
      Block
        CallExpr
          Callee:
            FieldExpr field='tick'
              Identifier name='scheduler'
        CallExpr
          Callee:
            Identifier name='pic_eoi'
          Args:
            IntLiteral value=0
  FuncDecl name='keyboard_handler' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='port' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=96
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u8'
        VarDecl name='scancode' is_const=false is_volatile=false
          TypeExpr kind=named name='u8'
          Init:
            UnaryExpr op=*
              Identifier name='port'
        CallExpr
          Callee:
            FieldExpr field='push'
              Identifier name='keyboard'
          Args:
            Identifier name='scancode'
        CallExpr
          Callee:
            Identifier name='pic_eoi'
          Args:
            IntLiteral value=1
  FuncDecl name='entry_point' realm=stack is_pub=true is_main=false
    Body:
      Block
        VarDecl name='len' is_const=false is_volatile=false
          TypeExpr kind=named name='u64'
          Init:
            BinaryExpr op=-
              Identifier name='BSS_END'
              Identifier name='BSS_START'
        LoopStmt
          Body:
            Block
              UnsafeBlock
                Body:
                  Block
                    AsmExpr code='cli; hlt' output='(none)'
raul@raul:~$
## test 08: #json out of v0.1 
## test 09: 
raul@raul:~$ ./ast_tool src/tests/samples/09_modules.runes
AST for src/tests/samples/09_modules.runes:
Program
  UseDecl
    Identifier name='kernel'
  UseDecl
    Identifier name='scheduler'
  UseDecl
    Identifier name='kernel'
    Identifier name='alloc_page'
  UseDecl
    Identifier name='kernel'
    Identifier name='free_page'
  UseDecl
    Identifier name='kernel'
    Identifier name='arch'
    Identifier name='x86'
    Identifier name='read_cr3'
  UseDecl
    Identifier name='kernel'
    Identifier name='arch'
    Identifier name='x86'
    Identifier name='write_cr3'
  UseDecl
    Identifier name='kernel'
    Identifier name='arch'
    Identifier name='x86'
    Identifier name='read_cr2'
  ModDecl name='memory' is_pub=false
    FuncDecl name='alloc_page' realm=regional is_pub=true is_main=false
      Return name='r'
        TypeExpr kind=fallible
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  Identifier name='bump_alloc'
                Args:
                  IntLiteral value=4096
          IfStmt
            Condition:
              BinaryExpr op===
                Identifier name='r'
                CastExpr kind=0
                  Expr:
                    IntLiteral value=0
                  Target Type:
                    TypeExpr kind=pointer
                      TypeExpr kind=named name='u8'
            Then:
              Block
                Assign
                  Target:
                    Identifier name='r'
                  Value:
                    ErrorExpr
                      Identifier name='MemError'
                      Identifier name='OutOfMemory'
    FuncDecl name='free_page' realm=stack is_pub=true is_main=false
      Params:
        Param name='p'
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
      Body:
        Block
          CallExpr
            Callee:
              Identifier name='raw_free'
            Args:
              Identifier name='p'
    FuncDecl name='get_heap_top' realm=stack is_pub=false is_main=false
      Return name='r'
        TypeExpr kind=named name='u64'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              Identifier name='heap_cursor'
    FuncDecl name='align_up' realm=stack is_pub=false is_main=false
      Params:
        Param name='addr'
          TypeExpr kind=named name='u64'
        Param name='align'
          TypeExpr kind=named name='u64'
      Return name='r'
        TypeExpr kind=named name='u64'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op=&
                BinaryExpr op=-
                  BinaryExpr op=+
                    Identifier name='addr'
                    Identifier name='align'
                  IntLiteral value=1
                BinaryExpr op=-
                  IntLiteral value=0
                  Identifier name='align'
  ModDecl name='scheduler' is_pub=false
    TypeDecl name='Task' is_pub=false
      FieldDecl name='id' is_volatile=false
        TypeExpr kind=named name='i32'
      FieldDecl name='name' is_volatile=false
        TypeExpr kind=named name='str'
      FieldDecl name='state' is_volatile=false
        TypeExpr kind=named name='i32'
    FuncDecl name='add' realm=stack is_pub=true is_main=false
      Params:
        Param name='t'
          TypeExpr kind=named name='Task'
      Body:
        Block
          CallExpr
            Callee:
              Identifier name='task_queue_push'
            Args:
              Identifier name='t'
    FuncDecl name='run_next' realm=stack is_pub=true is_main=false
      Return name='r'
        TypeExpr kind=named name='bool'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op=>
                CallExpr
                  Callee:
                    Identifier name='task_queue_len'
                IntLiteral value=0
          IfStmt
            Condition:
              Identifier name='r'
            Then:
              Block
                VarDecl name='t' is_const=false is_volatile=false
                  TypeExpr kind=named name='Task'
                  Init:
                    CallExpr
                      Callee:
                        Identifier name='task_queue_pop'
                Assign
                  Target:
                    FieldExpr field='state'
                      Identifier name='t'
                  Value:
                    IntLiteral value=1
                CallExpr
                  Callee:
                    FieldExpr field='run'
                      Identifier name='t'
    FuncDecl name='tick' realm=stack is_pub=true is_main=false
      Body:
        Block
          CallExpr
            Callee:
              Identifier name='preempt_current'
    FuncDecl name='preempt_current' realm=stack is_pub=false is_main=false
      Body:
        Block
          VarDecl name='cur' is_const=false is_volatile=false
            TypeExpr kind=named name='Task'
            Init:
              CallExpr
                Callee:
                  Identifier name='task_queue_current'
          Assign
            Target:
              FieldExpr field='state'
                Identifier name='cur'
            Value:
              IntLiteral value=0
          CallExpr
            Callee:
              Identifier name='task_queue_push'
            Args:
              Identifier name='cur'
          CallExpr
            Callee:
              Identifier name='run_next'
    FuncDecl name='idle' realm=stack is_pub=false is_main=false
      Body:
        Block
          UnsafeBlock
            Body:
              Block
                AsmExpr code='hlt' output='(none)'
  ModDecl name='io' is_pub=false
    FuncDecl name='write_byte' realm=stack is_pub=true is_main=false
      Params:
        Param name='port'
          TypeExpr kind=named name='u16'
        Param name='val'
          TypeExpr kind=named name='u8'
      Body:
        Block
          UnsafeBlock
            Body:
              Block
                AsmExpr code='outb %al, %dx' output='(none)'
    FuncDecl name='read_byte' realm=stack is_pub=true is_main=false
      Params:
        Param name='port'
          TypeExpr kind=named name='u16'
      Return name='r'
        TypeExpr kind=named name='u8'
      Body:
        Block
          UnsafeBlock
            Body:
              Block
                AsmExpr code='inb %dx, %al' output='r'
    FuncDecl name='write_word' realm=stack is_pub=true is_main=false
      Params:
        Param name='port'
          TypeExpr kind=named name='u16'
        Param name='val'
          TypeExpr kind=named name='u16'
      Body:
        Block
          UnsafeBlock
            Body:
              Block
                AsmExpr code='outw %ax, %dx' output='(none)'
    FuncDecl name='read_word' realm=stack is_pub=true is_main=false
      Params:
        Param name='port'
          TypeExpr kind=named name='u16'
      Return name='r'
        TypeExpr kind=named name='u16'
      Body:
        Block
          UnsafeBlock
            Body:
              Block
                AsmExpr code='inw %dx, %ax' output='r'
  ModDecl name='kernel' is_pub=false
    ModDecl name='arch' is_pub=false
      ModDecl name='x86' is_pub=false
        FuncDecl name='read_cr3' realm=stack is_pub=true is_main=false
          Return name='r'
            TypeExpr kind=named name='u64'
          Body:
            Block
              AsmExpr code='mov %cr3, %rax' output='r'
        FuncDecl name='write_cr3' realm=stack is_pub=true is_main=false
          Params:
            Param name='val'
              TypeExpr kind=named name='u64'
          Body:
            Block
              AsmExpr code='mov %rax, %cr3' output='(none)'
        FuncDecl name='read_cr2' realm=stack is_pub=true is_main=false
          Return name='r'
            TypeExpr kind=named name='u64'
          Body:
            Block
              AsmExpr code='mov %cr2, %rax' output='r'
        FuncDecl name='halt' realm=stack is_pub=true is_main=false
          Body:
            Block
              AsmExpr code='cli; hlt' output='(none)'
    FuncDecl name='alloc_page' realm=regional is_pub=true is_main=false
      Return name='r'
        TypeExpr kind=fallible
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              CallExpr
                Callee:
                  FieldExpr field='alloc_page'
                    Identifier name='memory'
    FuncDecl name='free_page' realm=stack is_pub=true is_main=false
      Params:
        Param name='p'
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
      Body:
        Block
          CallExpr
            Callee:
              FieldExpr field='free_page'
                Identifier name='memory'
            Args:
              Identifier name='p'
  FuncDecl name='demo_modules' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='page' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CatchExpr err_name='e'
              Expr:
                CallExpr
                  Callee:
                    FieldExpr field='alloc_page'
                      Identifier name='memory'
              Handler:
                Block
                  CallExpr
                    Callee:
                      Identifier name='print'
                    Args:
                      StringLiteral value="alloc failed:"
                      Identifier name='e'
                  ReturnStmt
        VarDecl name='cr3' is_const=false is_volatile=false
          TypeExpr kind=named name='u64'
          Init:
            CallExpr
              Callee:
                Identifier name='read_cr3'
        FieldExpr field='Task'
          Identifier name='scheduler'
        Assign
          Target:
            Identifier name='t'
          Value:
            CallExpr
              Callee:
                FieldExpr field='Task'
                  Identifier name='scheduler'
              Args:
                NamedArg name='id'
                  IntLiteral value=1
                NamedArg name='name'
                  StringLiteral value="init"
                NamedArg name='state'
                  IntLiteral value=0
        CallExpr
          Callee:
            FieldExpr field='add'
              Identifier name='scheduler'
          Args:
            Identifier name='t'
        CallExpr
          Callee:
            FieldExpr field='run_next'
              Identifier name='scheduler'
        CallExpr
          Callee:
            FieldExpr field='free_page'
              Identifier name='memory'
          Args:
            Identifier name='page'
        CallExpr
          Callee:
            FieldExpr field='write_byte'
              Identifier name='io'
          Args:
            IntLiteral value=1016
            CastExpr kind=0
              Expr:
                CharLiteral codepoint=U+0048
              Target Type:
                TypeExpr kind=named name='u8'
        CallExpr
          Callee:
            FieldExpr field='write_byte'
              Identifier name='io'
          Args:
            IntLiteral value=1016
            CastExpr kind=0
              Expr:
                CharLiteral codepoint=U+0069
              Target Type:
                TypeExpr kind=named name='u8'
        VarDecl name='status' is_const=false is_volatile=false
          TypeExpr kind=named name='u8'
          Init:
            CallExpr
              Callee:
                FieldExpr field='read_byte'
                  Identifier name='io'
              Args:
                IntLiteral value=1016
raul@raul:~$
## test 10:  
raul@raul:~$ ./ast_tool src/tests/samples/10_kernel_bootstrap.runes
[Error] src/tests/samples/10_kernel_bootstrap.runes:87:1: expected a declaration
[Error] src/tests/samples/10_kernel_bootstrap.runes:87:7: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:90:1: expected a declaration
[Error] src/tests/samples/10_kernel_bootstrap.runes:156:19: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:156:21: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:157:16: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:160:10: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:161:5: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:162:1: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:183:30: expected ')' after arguments
[Error] src/tests/samples/10_kernel_bootstrap.runes:183:36: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:183:41: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:184:43: expected '->' after match pattern
[Error] src/tests/samples/10_kernel_bootstrap.runes:185:36: expected '->' after match pattern
[Error] src/tests/samples/10_kernel_bootstrap.runes:213:35: expected '{'
[Error] src/tests/samples/10_kernel_bootstrap.runes:219:22: expected ')' after arguments
[Error] src/tests/samples/10_kernel_bootstrap.runes:219:29: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:222:7: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:223:18: expected error set name
[Error] src/tests/samples/10_kernel_bootstrap.runes:224:5: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:225:1: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:242:20: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:247:30: expected ')' after arguments
[Error] src/tests/samples/10_kernel_bootstrap.runes:247:39: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:248:42: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:248:47: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:249:42: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:249:47: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:250:42: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:253:13: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:254:9: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:256:5: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:262:1: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:305:31: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:308:34: expected ')' after arguments
[Error] src/tests/samples/10_kernel_bootstrap.runes:308:57: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:331:30: expected ')' after arguments
[Error] src/tests/samples/10_kernel_bootstrap.runes:331:40: expected an expression
[Error] src/tests/samples/10_kernel_bootstrap.runes:333:17: expected an expression
AST for src/tests/samples/10_kernel_bootstrap.runes:
Program
  UseDecl
    Identifier name='kernel'
    Identifier name='arch'
    Identifier name='x86'
  ExternDecl name='memset' is_func=true
    Params:
      Param name='ptr'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='val'
        TypeExpr kind=named name='i32'
      Param name='len'
        TypeExpr kind=named name='usize'
  ExternDecl name='memcpy' is_func=true
    Params:
      Param name='dst'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='src'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
  ExternDecl name='memcmp' is_func=true
    Params:
      Param name='a'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='b'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='usize'
    Return name='r'
      TypeExpr kind=named name='i32'
  ExternDecl name='BSS_START' is_func=false
    TypeExpr kind=named name='u64'
  ExternDecl name='BSS_END' is_func=false
    TypeExpr kind=named name='u64'
  ExternDecl name='KERNEL_START' is_func=false
    TypeExpr kind=named name='u64'
  ExternDecl name='KERNEL_END' is_func=false
    TypeExpr kind=named name='u64'
  ErrorDecl name='MapError' is_pub=false
    Identifier name='AlreadyMapped'
    Identifier name='InvalidAddress'
    Identifier name='OutOfMemory'
  ErrorDecl name='TaskError' is_pub=false
    Identifier name='SpawnFailed'
    Identifier name='InvalidEntry'
    Identifier name='QueueFull'
  ErrorDecl name='IoError' is_pub=false
    Identifier name='Timeout'
    Identifier name='BadPort'
    Identifier name='Overrun'
  TypeDecl name='PageTable' is_pub=false
    FieldDecl name='entries' is_volatile=false
      TypeExpr kind=array
        TypeExpr kind=named name='u64'
        Size:
          IntLiteral value=512
  TypeDecl name='SyscallFrame' is_pub=false
    FieldDecl name='rax' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rbx' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rcx' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rdx' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rsi' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rdi' is_volatile=false
      TypeExpr kind=named name='u64'
  TypeDecl name='InterruptFrame' is_pub=false
    FieldDecl name='rip' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='cs' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rflags' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='rsp' is_volatile=false
      TypeExpr kind=named name='u64'
    FieldDecl name='ss' is_volatile=false
      TypeExpr kind=named name='u64'
  TypeDecl name='Task' is_pub=false
    FieldDecl name='id' is_volatile=false
      TypeExpr kind=named name='i32'
    FieldDecl name='name' is_volatile=false
      TypeExpr kind=named name='str'
    FieldDecl name='state' is_volatile=false
      TypeExpr kind=named name='i32'
  VarDecl name='KERNEL_STACK' is_const=false is_volatile=false
    TypeExpr kind=named name='u8'
    Init:
      ArrayLiteral
  VarDecl name='GDT' is_const=false is_volatile=false
    TypeExpr kind=array
      TypeExpr kind=named name='u64'
      Size:
        IntLiteral value=8
    Init:
      ArrayLiteral
  MethodDecl type_name='PageTable' iface_name='(null)' is_pub=false
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
          IfStmt
            Condition:
              BinaryExpr op=!=
                BinaryExpr op=&
                  Identifier name='vaddr'
                  IntLiteral value=4095
                IntLiteral value=0
            Then:
              Block
                Assign
                  Target:
                    Identifier name='r'
                  Value:
                    ErrorExpr
                      Identifier name='MapError'
                      Identifier name='InvalidAddress'
                ReturnStmt
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
    FuncDecl name='unmap' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='vaddr'
          TypeExpr kind=named name='u64'
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
          Assign
            Target:
              IndexExpr
                Target:
                  FieldExpr field='entries'
                    Identifier name='self'
                Index:
                  Identifier name='idx'
            Value:
              IntLiteral value=0
    FuncDecl name='is_mapped' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='vaddr'
          TypeExpr kind=named name='u64'
      Return name='r'
        TypeExpr kind=named name='bool'
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
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op=!=
                IndexExpr
                  Target:
                    FieldExpr field='entries'
                      Identifier name='self'
                  Index:
                    Identifier name='idx'
                IntLiteral value=0
  FuncDecl name='uart_putc' realm=stack is_pub=false is_main=false
    Params:
      Param name='ch'
        TypeExpr kind=named name='u8'
    Body:
      Block
        VarDecl name='uart' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u32'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=268435456
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u32'
        Assign
          Target:
            UnaryExpr op=*
              Identifier name='uart'
          Value:
            CastExpr kind=0
              Expr:
                Identifier name='ch'
              Target Type:
                TypeExpr kind=named name='u32'
  FuncDecl name='uart_puts' realm=stack is_pub=false is_main=false
    Params:
      Param name='s'
        TypeExpr kind=named name='str'
    Body:
      Block
        ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
          Iter:
            RangeExpr inclusive=false
              Start:
                IntLiteral value=0
              End:
                FieldExpr field='len'
                  Identifier name='s'
          Body:
            Block
              CallExpr
                Callee:
                  Identifier name='uart_putc'
                Args:
                  UnaryExpr op=*
                    BinaryExpr op=+
                      FieldExpr field='ptr'
                        Identifier name='s'
                      Identifier name='i'
  FuncDecl name='uart_ready' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=named name='bool'
    Body:
      Block
        VarDecl name='lsr' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=268435461
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u8'
        Assign
          Target:
            Identifier name='r'
          Value:
            BinaryExpr op=!=
              BinaryExpr op=&
                UnaryExpr op=*
                  Identifier name='lsr'
                IntLiteral value=32
              IntLiteral value=0
  FuncDecl name='page_fault_handler' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='cr2' is_const=false is_volatile=false
          TypeExpr kind=named name='u64'
          Init:
            CallExpr
              Callee:
                Identifier name='read_cr2'
        MatchStmt
          Subject:
            CallExpr
              Callee:
                Identifier name='handle_page_fault'
              Args:
                Identifier name='cr2'
          Arms:
        Identifier name='e'
        StringLiteral value="page fault: unhandled\n"
        LoopStmt
          Body:
            Block
              UnsafeBlock
                Body:
                  Block
                    AsmExpr code='cli; hlt' output='(none)'
  ArrayLiteral
    Identifier name='interrupt'
  FuncDecl name='double_fault_handler' realm=stack is_pub=false is_main=false
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='uart_puts'
          Args:
            StringLiteral value="DOUBLE FAULT\n"
        LoopStmt
          Body:
            Block
              UnsafeBlock
                Body:
                  Block
                    AsmExpr code='cli; hlt' output='(none)'
  FuncDecl name='timer_handler' realm=stack is_pub=false is_main=false
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='scheduler_tick'
        CallExpr
          Callee:
            Identifier name='pic_eoi'
          Args:
            IntLiteral value=0
  FuncDecl name='syscall_entry' realm=stack is_pub=false is_main=false
    Params:
      Param name='nr'
        TypeExpr kind=named name='u64'
      Param name='a'
        TypeExpr kind=named name='u64'
      Param name='b'
        TypeExpr kind=named name='u64'
      Param name='c'
        TypeExpr kind=named name='u64'
      Param name='d'
        TypeExpr kind=named name='u64'
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        MatchStmt
          Subject:
            Identifier name='nr'
          Arms:
  FuncDecl name='sys_read' realm=stack is_pub=false is_main=false
    Params:
      Param name='fd'
        TypeExpr kind=named name='u64'
      Param name='buf'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='u64'
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            IntLiteral value=0
  FuncDecl name='sys_write' realm=stack is_pub=false is_main=false
    Params:
      Param name='fd'
        TypeExpr kind=named name='u64'
      Param name='buf'
        TypeExpr kind=pointer
          TypeExpr kind=named name='u8'
      Param name='len'
        TypeExpr kind=named name='u64'
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
          Iter:
            RangeExpr inclusive=false
              Start:
                IntLiteral value=0
              End:
                Identifier name='len'
          Body:
            Block
              CallExpr
                Callee:
                  Identifier name='uart_putc'
                Args:
                  UnaryExpr op=*
                    BinaryExpr op=+
                      Identifier name='buf'
                      Identifier name='i'
        Assign
          Target:
            Identifier name='r'
          Value:
            Identifier name='len'
  FuncDecl name='sys_exit' realm=stack is_pub=false is_main=false
    Params:
      Param name='code'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='u64'
    Body:
      Block
        CallExpr
          Callee:
            Identifier name='uart_puts'
          Args:
            StringLiteral value="process exited\n"
        Assign
          Target:
            Identifier name='r'
          Value:
            CastExpr kind=0
              Expr:
                Identifier name='code'
              Target Type:
                TypeExpr kind=named name='u64'
  FuncDecl name='handle_page_fault' realm=stack is_pub=false is_main=false
    Params:
      Param name='addr'
        TypeExpr kind=named name='u64'
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='void'
    Body:
      Block
        VarDecl name='page' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CallExpr
              Callee:
                Identifier name='raw_alloc'
              Args:
                IntLiteral value=4096
        IfStmt
          Condition:
            BinaryExpr op===
              Identifier name='page'
              CastExpr kind=0
                Expr:
                  IntLiteral value=0
                Target Type:
                  TypeExpr kind=pointer
                    TypeExpr kind=named name='u8'
          Then:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  ErrorExpr
                    Identifier name='MapError'
                    Identifier name='OutOfMemory'
              ReturnStmt
        TupleExpr
          BinaryExpr op=&
            Identifier name='addr'
            IntLiteral value=18446744073709547520
          CastExpr kind=0
            Expr:
              Identifier name='page'
            Target Type:
              TypeExpr kind=named name='u64'
          IntLiteral value=7
  FuncDecl name='setup_paging' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=fallible
        TypeExpr kind=named name='void'
    Body:
      Block
        FuncDecl name='make_tables' realm=regional is_pub=false is_main=false
          Return name='out'
            TypeExpr kind=tuple
              TypeExpr kind=pointer
                TypeExpr kind=named name='PageTable'
              TypeExpr kind=pointer
                TypeExpr kind=named name='PageTable'
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
              VarDecl name='h_pml4' is_const=false is_volatile=false
                TypeExpr kind=pointer
                  TypeExpr kind=named name='PageTable'
                Init:
                  PromoteExpr target=dynamic
                    UnaryExpr op=&
                      Identifier name='pml4'
              VarDecl name='h_pdpt' is_const=false is_volatile=false
                TypeExpr kind=pointer
                  TypeExpr kind=named name='PageTable'
                Init:
                  PromoteExpr target=dynamic
                    UnaryExpr op=&
                      Identifier name='pdpt'
              Assign
                Target:
                  Identifier name='out'
                Value:
                  TupleExpr
                    Identifier name='h_pml4'
                    Identifier name='h_pdpt'
        VarDecl name='pml4' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='PageTable'
        VarDecl name='pdpt' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='PageTable'
          Init:
            CallExpr
              Callee:
                Identifier name='make_tables'
        VarDecl name='addr' is_const=false is_volatile=false
          TypeExpr kind=named name='u64'
          Init:
            IntLiteral value=0
        WhileStmt
          Condition:
            BinaryExpr op=<
              Identifier name='addr'
              IntLiteral value=2097152
          Body:
            Block
              Identifier name='_'
        FieldExpr field='AlreadyMapped'
          Identifier name='MapError'
  Identifier name='e'
  ReturnStmt
  TupleExpr
    IntLiteral value=18446603336221196288
    Identifier name='KERNEL_START'
    IntLiteral value=3
  UnsafeBlock
    Body:
      Block
        AsmExpr code='mov %rax, %cr3' output='(none)'
  FuncDecl name='scheduler_tick' realm=stack is_pub=false is_main=false
    Body:
      Block
        LoopStmt
          Body:
            Block
              IfStmt
                Condition:
                  CallExpr
                    Callee:
                      Identifier name='task_queue_empty'
                Then:
                  Block
                    BreakStmt
              VarDecl name='t' is_const=false is_volatile=false
                TypeExpr kind=named name='Task'
                Init:
                  CallExpr
                    Callee:
                      Identifier name='task_queue_pop'
              CallExpr
                Callee:
                  FieldExpr field='run'
                    Identifier name='t'
              CallExpr
                Callee:
                  Identifier name='task_queue_push'
                Args:
                  Identifier name='t'
              BreakStmt
  FuncDecl name='pic_eoi' realm=stack is_pub=false is_main=false
    Params:
      Param name='irq'
        TypeExpr kind=named name='u8'
    Body:
      Block
        VarDecl name='pic1' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=32
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u8'
        VarDecl name='pic2' is_const=false is_volatile=true
          TypeExpr kind=pointer
            TypeExpr kind=named name='u8'
          Init:
            CastExpr kind=0
              Expr:
                IntLiteral value=160
              Target Type:
                TypeExpr kind=pointer
                  TypeExpr kind=named name='u8'
        IfStmt
          Condition:
            BinaryExpr op=>=
              Identifier name='irq'
              IntLiteral value=8
          Then:
            Block
              Assign
                Target:
                  UnaryExpr op=*
                    Identifier name='pic2'
                Value:
                  IntLiteral value=32
        Assign
          Target:
            UnaryExpr op=*
              Identifier name='pic1'
          Value:
            IntLiteral value=32
  FuncDecl name='kernel_main' realm=stack is_pub=true is_main=false
    Body:
      Block
        CatchExpr err_name='e'
          Expr:
            CallExpr
              Callee:
                Identifier name='setup_paging'
          Handler:
            Block
              CallExpr
                Callee:
                  Identifier name='uart_puts'
                Args:
                  StringLiteral value="paging setup failed\n"
              LoopStmt
                Body:
                  Block
                    UnsafeBlock
                      Body:
                        Block
                          AsmExpr code='cli; hlt' output='(none)'
        CallExpr
          Callee:
            Identifier name='uart_puts'
          Args:
            StringLiteral value="paging OK\n"
        UnsafeBlock
          Body:
            Block
              AsmExpr code='sti' output='(none)'
        FuncDecl name='run_userspace' realm=gc is_pub=false is_main=false
          Body:
            Block
              FuncDecl name='validate_task' realm=stack is_pub=false is_main=false
                Params:
                  Param name='t'
                    TypeExpr kind=named name='Task'
                Return name='r'
                  TypeExpr kind=named name='bool'
                Body:
                  Block
                    Assign
                      Target:
                        Identifier name='r'
                      Value:
                        BinaryExpr op=>
                          FieldExpr field='id'
                            Identifier name='t'
                          IntLiteral value=0
                    VarDecl name='t' is_const=false is_volatile=false
                      TypeExpr kind=named name='and'
              IfStmt
                Condition:
                  CallExpr
                    Callee:
                      Identifier name='validate_task'
                    Args:
                      Identifier name='shell'
                Then:
                  Block
                    CallExpr
                      Callee:
                        Identifier name='task_queue_push'
                      Args:
                        Identifier name='shell'
              WhileStmt
                Condition:
                  BoolLiteral value=true
                Body:
                  Block
                    CallExpr
                      Callee:
                        Identifier name='scheduler_tick'
        CallExpr
          Callee:
            Identifier name='run_userspace'
  FuncDecl name='entry_point' realm=stack is_pub=true is_main=false
    Body:
      Block
        VarDecl name='bss_len' is_const=false is_volatile=false
          TypeExpr kind=named name='u64'
          Init:
            BinaryExpr op=-
              Identifier name='BSS_END'
              Identifier name='BSS_START'
        LoopStmt
          Body:
            Block
              UnsafeBlock
                Body:
                  Block
                    AsmExpr code='cli; hlt' output='(none)'
raul@raul:~$
## test 11:  
raul@raul:~$ ./ast_tool src/tests/samples/11_edge_cases.runes
[Error] src/tests/samples/11_edge_cases.runes:144:11: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:144:39: expected ')' after arguments
[Error] src/tests/samples/11_edge_cases.runes:144:40: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:145:15: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:145:44: expected ')' after arguments
[Error] src/tests/samples/11_edge_cases.runes:145:45: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:146:10: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:146:17: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:160:18: expected ')' after arguments
[Error] src/tests/samples/11_edge_cases.runes:160:25: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:175:9: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:176:24: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:181:31: expected ')' after arguments
[Error] src/tests/samples/11_edge_cases.runes:181:35: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:184:10: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:185:27: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:187:42: expected ')' after arguments
[Error] src/tests/samples/11_edge_cases.runes:187:46: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:189:9: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:190:22: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:191:19: expected a type expression
[Error] src/tests/samples/11_edge_cases.runes:191:36: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:194:9: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:196:5: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:197:1: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:211:9: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:216:1: expected an expression
[Error] src/tests/samples/11_edge_cases.runes:246:29: expected ')' after arguments
[Error] src/tests/samples/11_edge_cases.runes:246:30: expected an expression
AST for src/tests/samples/11_edge_cases.runes:
Program
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
  FuncDecl name='cube' realm=stack is_pub=false is_main=false
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
              BinaryExpr op=*
                Identifier name='x'
                Identifier name='x'
              Identifier name='x'
  FuncDecl name='negate' realm=stack is_pub=false is_main=false
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
              UnaryExpr op=-
                IntLiteral value=1
  FuncDecl name='is_zero' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='bool'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            BinaryExpr op===
              Identifier name='x'
              IntLiteral value=0
  FuncDecl name='identity' realm=stack is_pub=false is_main=false
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
  FuncDecl name='s_add' realm=stack is_pub=false is_main=false
    Params:
      Param name='a'
        TypeExpr kind=named name='i32'
      Param name='b'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            BinaryExpr op=+
              Identifier name='a'
              Identifier name='b'
  FuncDecl name='main' realm=stack is_pub=false is_main=true
    Body:
      Block
        FuncDecl name='prepare' realm=regional is_pub=false is_main=false
          Body:
            Block
              FuncDecl name='helper' realm=stack is_pub=false is_main=false
                Params:
                  Param name='p'
                    TypeExpr kind=pointer
                      TypeExpr kind=named name='u8'
                Body:
                  Block
                    Assign
                      Target:
                        UnaryExpr op=*
                          Identifier name='p'
                      Value:
                        IntLiteral value=255
              VarDecl name='pt' is_const=false is_volatile=false
                TypeExpr kind=named name='PageTable'
                Init:
                  CallExpr
                    Callee:
                      Identifier name='PageTable'
                    Args:
                      NamedArg name='entries'
                        ArrayLiteral
              CallExpr
                Callee:
                  Identifier name='helper'
                Args:
                  UnaryExpr op=&
                    CastExpr kind=0
                      Expr:
                        Identifier name='pt'
                      Target Type:
                        TypeExpr kind=pointer
                          TypeExpr kind=named name='u8'
        CallExpr
          Callee:
            Identifier name='prepare'
        FuncDecl name='load' realm=dynamic is_pub=false is_main=false
          Return name='r'
            TypeExpr kind=pointer
              TypeExpr kind=named name='Config'
          Body:
            Block
              FuncDecl name='validate' realm=stack is_pub=false is_main=false
                Params:
                  Param name='s'
                    TypeExpr kind=named name='str'
                Return name='ok'
                  TypeExpr kind=named name='bool'
                Body:
                  Block
                    Assign
                      Target:
                        Identifier name='ok'
                      Value:
                        BinaryExpr op=>
                          FieldExpr field='len'
                            Identifier name='s'
                          IntLiteral value=0
              FuncDecl name='fetch' realm=gc is_pub=false is_main=false
                Params:
                  Param name='name'
                    TypeExpr kind=named name='str'
                Return name='c'
                  TypeExpr kind=pointer
                    TypeExpr kind=named name='Config'
                Body:
                  Block
                    Assign
                      Target:
                        Identifier name='c'
                      Value:
                        CallExpr
                          Callee:
                            Identifier name='config_load'
                          Args:
                            Identifier name='name'
              IfStmt
                Condition:
                  CallExpr
                    Callee:
                      Identifier name='validate'
                    Args:
                      StringLiteral value="default"
                Then:
                  Block
                    Assign
                      Target:
                        Identifier name='r'
                      Value:
                        CallExpr
                          Callee:
                            Identifier name='fetch'
                          Args:
                            StringLiteral value="default"
                Else:
                  Block
                    Assign
                      Target:
                        Identifier name='r'
                      Value:
                        CastExpr kind=0
                          Expr:
                            IntLiteral value=0
                          Target Type:
                            TypeExpr kind=pointer
                              TypeExpr kind=named name='Config'
        VarDecl name='cfg' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='Config'
          Init:
            CallExpr
              Callee:
                Identifier name='load'
        FuncDecl name='run' realm=gc is_pub=false is_main=false
          Params:
            Param name='c'
              TypeExpr kind=pointer
                TypeExpr kind=named name='Config'
          Body:
            Block
              FuncDecl name='parse_rules' realm=gc is_pub=false is_main=false
                Params:
                  Param name='c'
                    TypeExpr kind=pointer
                      TypeExpr kind=named name='Config'
                Return name='r'
                  TypeExpr kind=pointer
                    TypeExpr kind=named name='RuleSet'
                Body:
                  Block
                    Assign
                      Target:
                        Identifier name='r'
                      Value:
                        CallExpr
                          Callee:
                            Identifier name='ruleset_from_config'
                          Args:
                            Identifier name='c'
              VarDecl name='rs' is_const=false is_volatile=false
                TypeExpr kind=pointer
                  TypeExpr kind=named name='RuleSet'
                Init:
                  CallExpr
                    Callee:
                      Identifier name='parse_rules'
                    Args:
                      Identifier name='c'
              CallExpr
                Callee:
                  Identifier name='apply_rules'
                Args:
                  Identifier name='rs'
        CallExpr
          Callee:
            Identifier name='run'
          Args:
            Identifier name='cfg'
  FuncDecl name='split_promote' realm=regional is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=tuple
        TypeExpr kind=pointer
          TypeExpr kind=named name='Node'
        TypeExpr kind=pointer
          TypeExpr kind=named name='Node'
    Body:
      Block
        VarDecl name='a' is_const=false is_volatile=false
          TypeExpr kind=named name='Node'
          Init:
            CallExpr
              Callee:
                Identifier name='Node'
              Args:
                NamedArg name='val'
                  IntLiteral value=1
                NamedArg name='next'
                  CastExpr kind=0
                    Expr:
                      IntLiteral value=0
                    Target Type:
                      TypeExpr kind=pointer
                        TypeExpr kind=named name='Node'
        VarDecl name='b' is_const=false is_volatile=false
          TypeExpr kind=named name='Node'
          Init:
            CallExpr
              Callee:
                Identifier name='Node'
              Args:
                NamedArg name='val'
                  IntLiteral value=2
                NamedArg name='next'
                  CastExpr kind=0
                    Expr:
                      IntLiteral value=0
                    Target Type:
                      TypeExpr kind=pointer
                        TypeExpr kind=named name='Node'
        VarDecl name='a_dyn' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='Node'
          Init:
            PromoteExpr target=dynamic
              UnaryExpr op=&
                Identifier name='a'
        VarDecl name='b_gc' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='Node'
          Init:
            PromoteExpr target=gc
              UnaryExpr op=&
                Identifier name='b'
        Assign
          Target:
            Identifier name='r'
          Value:
            TupleExpr
              Identifier name='a_dyn'
              Identifier name='b_gc'
  FuncDecl name='sign' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='str'
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            IfStmt
              Condition:
                BinaryExpr op=>
                  Identifier name='x'
                  IntLiteral value=0
              Then:
                Block
                  StringLiteral value="positive"
              Else:
                IfStmt
                  Condition:
                    BinaryExpr op=<
                      Identifier name='x'
                      IntLiteral value=0
                  Then:
                    Block
                      StringLiteral value="negative"
                  Else:
                    Block
                      StringLiteral value="zero"
  FuncDecl name='max3' realm=stack is_pub=false is_main=false
    Params:
      Param name='a'
        TypeExpr kind=named name='i32'
      Param name='b'
        TypeExpr kind=named name='i32'
      Param name='c'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=named name='i32'
    Body:
      Block
        VarDecl name='ab' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            IfStmt
              Condition:
                BinaryExpr op=>
                  Identifier name='a'
                  Identifier name='b'
              Then:
                Block
                  Identifier name='a'
              Else:
                Block
                  Identifier name='b'
        Assign
          Target:
            Identifier name='r'
          Value:
            IfStmt
              Condition:
                BinaryExpr op=>
                  Identifier name='ab'
                  Identifier name='c'
              Then:
                Block
                  Identifier name='ab'
              Else:
                Block
                  Identifier name='c'
  FuncDecl name='minmax' realm=stack is_pub=false is_main=false
    Params:
      Param name='a'
        TypeExpr kind=named name='i32'
      Param name='b'
        TypeExpr kind=named name='i32'
    Return name='r'
      TypeExpr kind=tuple
        TypeExpr kind=named name='i32'
        TypeExpr kind=named name='i32'
    Body:
      Block
        IfStmt
          Condition:
            BinaryExpr op=<
              Identifier name='a'
              Identifier name='b'
          Then:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  TupleExpr
                    Identifier name='a'
                    Identifier name='b'
          Else:
            Block
              Assign
                Target:
                  Identifier name='r'
                Value:
                  TupleExpr
                    Identifier name='b'
                    Identifier name='a'
  FuncDecl name='demo_tuple_destruct' realm=stack is_pub=false is_main=false
    Body:
      Block
        VarDecl name='lo' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
        VarDecl name='left' is_const=false is_volatile=false
          TypeExpr kind=pointer
            TypeExpr kind=named name='Node'
        VarDecl name='x' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
        VarDecl name='y' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
        VarDecl name='z' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            CallExpr
              Callee:
                Identifier name='get_coords'
  FuncDecl name='transform_buffer' realm=stack is_pub=false is_main=false
    Params:
      Param name='buf'
        TypeExpr kind=array
          TypeExpr kind=named name='i32'
          Size:
            IntLiteral value=16
      Param name='scale'
        TypeExpr kind=named name='i32'
      Param name='offset'
        TypeExpr kind=named name='i32'
    Body:
      Block
        ForStmt cap_kind=2 cap_value='n' cap_index='i'
          Iter:
            Identifier name='buf'
          Body:
            Block
              VarDecl name='old' is_const=false is_volatile=false
                TypeExpr kind=named name='i32'
                Init:
                  UnaryExpr op=*
                    Identifier name='n'
              Assign
                Target:
                  UnaryExpr op=*
                    Identifier name='n'
                Value:
                  BinaryExpr op=+
                    BinaryExpr op=*
                      Identifier name='old'
                      Identifier name='scale'
                    Identifier name='offset'
              IfStmt
                Condition:
                  BinaryExpr op=<
                    UnaryExpr op=*
                      Identifier name='n'
                    IntLiteral value=0
                Then:
                  Block
                    Assign
                      Target:
                        UnaryExpr op=*
                          Identifier name='n'
                      Value:
                        IntLiteral value=0
              UnaryExpr op=*
                Identifier name='n'
  VariantDecl name='Event' is_pub=false
    VariantArm name='KeyPress'
      TypeExpr kind=named name='u8'
    VariantArm name='MouseMove'
      TypeExpr kind=named name='i32'
      TypeExpr kind=named name='i32'
    VariantArm name='Resize'
      TypeExpr kind=named name='u32'
      TypeExpr kind=named name='u32'
    VariantArm name='Quit'
  FuncDecl name='handle_event' realm=stack is_pub=false is_main=false
    Params:
      Param name='ev'
        TypeExpr kind=named name='Event'
    Return name='r'
      TypeExpr kind=named name='bool'
    Body:
      Block
        Identifier name='code'
        IfStmt
          Condition:
            BinaryExpr op===
              Identifier name='code'
              IntLiteral value=27
          Then:
            Block
              CallExpr
                Callee:
                  Identifier name='print'
                Args:
                  StringLiteral value="escape pressed"
              BoolLiteral value=false
          Else:
            Block
  TupleExpr
    Identifier name='dx'
    Identifier name='dy'
  VarDecl name='dist' is_const=false is_volatile=false
    TypeExpr kind=named name='i32'
    Init:
      BinaryExpr op=+
        BinaryExpr op=*
          Identifier name='dx'
          Identifier name='dx'
        BinaryExpr op=*
          Identifier name='dy'
          Identifier name='dy'
  TupleExpr
    Identifier name='w'
    Identifier name='h'
  TupleExpr
    Identifier name='w'
    Identifier name='h'
  BoolLiteral value=true
  FuncDecl name='dual_match' realm=stack is_pub=false is_main=false
    Params:
      Param name='x'
        TypeExpr kind=named name='i32'
      Param name='color'
        TypeExpr kind=named name='Color'
    Return name='r'
      TypeExpr kind=named name='str'
    Body:
      Block
        MatchStmt
          Subject:
            Identifier name='x'
          Arms:
            MatchArm
              Pattern:
                IntLiteral value=0
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="zero branch"
            MatchArm
              Pattern:
                Identifier name='_'
              Body:
                CallExpr
                  Callee:
                    Identifier name='print'
                  Args:
                    StringLiteral value="nonzero branch"
  VariantDecl name='Color' is_pub=false
    VariantArm name='Red'
    VariantArm name='Green'
    VariantArm name='Blue'
  FuncDecl name='rotate_left' realm=stack is_pub=false is_main=false
    Params:
      Param name='arr'
        TypeExpr kind=array
          TypeExpr kind=named name='i32'
          Size:
            IntLiteral value=8
    Body:
      Block
        VarDecl name='first' is_const=false is_volatile=false
          TypeExpr kind=named name='i32'
          Init:
            IndexExpr
              Target:
                Identifier name='arr'
              Index:
                IntLiteral value=0
        ForStmt cap_kind=0 cap_value='i' cap_index='(null)'
          Iter:
            RangeExpr inclusive=false
              Start:
                IntLiteral value=0
              End:
                IntLiteral value=7
          Body:
            Block
              Assign
                Target:
                  IndexExpr
                    Target:
                      Identifier name='arr'
                    Index:
                      Identifier name='i'
                Value:
                  IndexExpr
                    Target:
                      Identifier name='arr'
                    Index:
                      BinaryExpr op=+
                        Identifier name='i'
                        IntLiteral value=1
        Assign
          Target:
            IndexExpr
              Target:
                Identifier name='arr'
              Index:
                IntLiteral value=7
          Value:
            Identifier name='first'
  FuncDecl name='init_magic_bytes' realm=stack is_pub=false is_main=false
    Return name='r'
      TypeExpr kind=array
        TypeExpr kind=named name='u8'
        Size:
          IntLiteral value=8
    Body:
      Block
        Assign
          Target:
            Identifier name='r'
          Value:
            ArrayLiteral
              IntLiteral value=137
              IntLiteral value=80
              IntLiteral value=78
              IntLiteral value=71
              IntLiteral value=13
              IntLiteral value=10
              IntLiteral value=26
              IntLiteral value=10
  FuncDecl name='chained_fallback' realm=stack is_pub=false is_main=false
    Params:
      Param name='a'
        TypeExpr kind=named name='f32'
      Param name='b'
        TypeExpr kind=named name='f32'
      Param name='c'
        TypeExpr kind=named name='f32'
    Return name='r'
      TypeExpr kind=named name='f32'
    Body:
      Block
        CatchExpr err_name='(null)'
          Expr:
            TupleExpr
              Identifier name='a'
              Identifier name='c'
          Handler:
            FloatLiteral value=0.000000
        Assign
          Target:
            Identifier name='r'
          Value:
            Identifier name='primary'
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
  ErrorDecl name='MathError' is_pub=false
    Identifier name='DivByZero'
  TypeDecl name='Counter' is_pub=false
    FieldDecl name='value' is_volatile=false
      TypeExpr kind=named name='i32'
    FieldDecl name='name' is_volatile=false
      TypeExpr kind=named name='str'
  MethodDecl type_name='Counter' iface_name='(null)' is_pub=false
    FuncDecl name='reset' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Body:
        Block
          Assign
            Target:
              FieldExpr field='value'
                Identifier name='self'
            Value:
              IntLiteral value=0
    FuncDecl name='increment' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Body:
        Block
          Assign
            Target:
              FieldExpr field='value'
                Identifier name='self'
            Value:
              BinaryExpr op=+
                FieldExpr field='value'
                  Identifier name='self'
                IntLiteral value=1
    FuncDecl name='add' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
        Param name='n'
          TypeExpr kind=named name='i32'
      Body:
        Block
          Assign
            Target:
              FieldExpr field='value'
                Identifier name='self'
            Value:
              BinaryExpr op=+
                FieldExpr field='value'
                  Identifier name='self'
                Identifier name='n'
    FuncDecl name='get' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='i32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              FieldExpr field='value'
                Identifier name='self'
    FuncDecl name='is_zero' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=named name='bool'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              BinaryExpr op===
                FieldExpr field='value'
                  Identifier name='self'
                IntLiteral value=0
    FuncDecl name='snapshot' realm=stack is_pub=false is_main=false
      Params:
        Param name='self'
      Return name='r'
        TypeExpr kind=tuple
          TypeExpr kind=named name='str'
          TypeExpr kind=named name='i32'
      Body:
        Block
          Assign
            Target:
              Identifier name='r'
            Value:
              TupleExpr
                FieldExpr field='name'
                  Identifier name='self'
                FieldExpr field='value'
                  Identifier name='self'
raul@raul:~$
