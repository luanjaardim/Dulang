use inkwell::basic_block::BasicBlock;
use inkwell::builder::{Builder, BuilderError};
use inkwell::context::Context;
use inkwell::module::Module;
use inkwell::types::{AnyTypeEnum, BasicMetadataTypeEnum, BasicType, BasicTypeEnum, FunctionType, PointerType};
use inkwell::values::{AnyValue, AnyValueEnum, ArrayValue, BasicMetadataValueEnum, BasicValue, BasicValueEnum, CallSiteValue, FunctionValue, FloatValue, IntValue, PointerValue};
use inkwell::{AddressSpace, IntPredicate};
use crate::grammar::ASTNode;
use crate::tokenizer::TokenType;
use crate::visitor::Var;
use crate::{visitor::{Scope, ScopeAttr, Visitor, Elem, ExprType}, grammar::Node, tokenizer::Token};

use std::collections::HashMap;
use std::error::Error;
use std::ffi::CStr;
use std::io::Write;

pub enum CodeGenError<'n> {
    NotImplemented(&'n Node),
}

#[derive(Debug)]
pub enum DefType<'ctx> {
    Var(PointerValue<'ctx>),
    Fn(FunctionValue<'ctx>),
    Const(BasicValueEnum<'ctx>),
    Empty,
}
impl<'ctx> DefType<'ctx> {
    fn get_var(&self) -> &PointerValue<'ctx> {
        match self {
            DefType::Var(pnt) => pnt,
            _ => panic!("Using get_var on a DefType that is not a Var")
        }
    }
    fn get_fn(&self) -> &FunctionValue<'ctx> {
        match self {
            DefType::Fn(func) => func,
            _ => panic!("Using get_fn on a DefType that is not a Fn")
        }
    }
    fn get_const(&self) -> &BasicValueEnum<'ctx> {
        match self {
            DefType::Const(constant) => constant,
            _ => panic!("Using get_const on a DefType that is not a Const")
        }
    }
}

pub struct CodeGen<'ctx, 'ast, 'vis> 
where
    'vis: 'ast
{
    ctx: &'ctx Context,
    module: Module<'ctx>,
    builder: Builder<'ctx>,
    cur_scp: &'vis Scope,
    cur_ind: Vec<usize>,
    ast: &'ast Vec<Node>,
    defs: HashMap<String, Vec<DefType<'ctx>>>,
}

impl<'ctx, 'ast, 'vis> CodeGen<'ctx, 'ast, 'vis> {

    pub fn new(ctx: &'ctx Context, ast: &'ast Vec<Node>, visitor: &'vis mut Visitor, module_name: &str) -> Result<Self, std::io::Error> {
        // NOTE: This function was created because sometimes the scp_father access crashes the
        // program, and so i am updating its values correctly before the use.
        fn update_scp_father(scp: &mut Scope) {
            let scp_ref = &*scp as *const Scope;
            for e in scp.elems.iter_mut() {
                if let Elem::Scope(s) = e {
                    s.scp_father = scp_ref;
                    update_scp_father(s)
                }
            }
        }
        update_scp_father(visitor.glob_scope.as_mut().unwrap());
        let cg = CodeGen {
            ctx,
            module: ctx.create_module(module_name),
            builder: ctx.create_builder(),
            cur_scp: visitor.glob_scope.as_ref().unwrap(),
            cur_ind: vec![0],
            ast,
            defs: HashMap::new(),
        };
        Ok(cg)
    }

    fn get_def(&self, name: &str) -> &DefType<'ctx> {
        self.defs[name].last().unwrap()
    }
    fn add_def(&mut self, name: &str, elem: DefType<'ctx>) {
        let def_name = self.next_def().get_name().unwrap();
        assert!(def_name == name); // Assert that every definition follows the correct order
        if let Some(v) = self.defs.get_mut(name) {
            v.push(elem)
        } else {
            self.defs.insert(name.to_string(), vec![elem]);
        }
    }
    fn clear_cur_scp_vars(&mut self) {
        // println!("{:?}", self.cur_scp);
        for e in self.cur_scp.elems.iter().rev() {
            let var_name = e.get_name().unwrap();
            // println!("{var_name}: {:?}", self.defs.get(var_name));
            self.defs.get_mut(var_name).unwrap().pop().expect("The vector was empty");
        }
        self.cur_ind.pop();
        self.cur_scp = unsafe { &*self.cur_scp.scp_father };
    }
    fn next_def(&mut self) -> &Elem {
        let last = self.cur_ind.last_mut().unwrap();
        *last += 1;
        let mut ret = &self.cur_scp.elems[*last-1];
        while let Elem::Captured(_) = ret {
            *self.cur_ind.last_mut().unwrap() += 1;
            ret = &self.cur_scp.elems[*self.cur_ind.last().unwrap()];
        }
        if let Elem::Scope(s) = ret {
            self.cur_ind.push(0);
            self.cur_scp = s;
        }
        ret
    }
    fn peek_def(&mut self) -> &Elem {
        let mut ret = &self.cur_scp.elems[*self.cur_ind.last().unwrap()];
        while let Elem::Captured(_) = ret {
            *self.cur_ind.last_mut().unwrap() += 1;
            ret = &self.cur_scp.elems[*self.cur_ind.last().unwrap()];
        }
        return ret
    }
    fn find_def(&self, name: &str) -> Option<&Elem> {
        let mut scp = self.cur_scp;
        for i in self.cur_ind.iter().rev() {
            let elem = scp.find_elem_type("any", name, Some(*i as isize), false, true);
            if elem.is_some() { return elem }
            scp = unsafe { &*scp.scp_father };
        }
        Option::None
    }
    fn get_func_type(&self, t: &ExprType, captured_vars_len: usize) -> FunctionType<'ctx> {
        if let ExprType::FnType(inner) = t {
            let (params_types, ret_type) = inner.split_at(inner.len()-1);
            let ptr_type: BasicMetadataTypeEnum<'ctx> = self.ctx.ptr_type(inkwell::AddressSpace::default()).into();
            let _params_types = if let ExprType::None = params_types[0] {
                vec![ptr_type; captured_vars_len]
            } else {
                 vec![ptr_type; captured_vars_len].into_iter()
                     .chain(
                         params_types.iter().map(|t| self.get_basic_type_metadata(t)).collect::<Vec<BasicMetadataTypeEnum>>().into_iter()
                     ).collect::<Vec<BasicMetadataTypeEnum>>()
            };
            if let ExprType::None = ret_type[0] {
                self.ctx.void_type().fn_type(&_params_types, false)
            } else {
                self.get_basic_type(&ret_type[0]).fn_type(&_params_types, false)
            }
        } else { panic!("Not a function type") }
    }
    fn compound_constant_types_aux(&self, expr: &Node, ind: usize) -> (BasicValueEnum<'ctx>, Vec<(Vec<usize>, BasicValueEnum<'ctx>)>) {
        match &*expr.v {
            ASTNode::Array(elems) | ASTNode::Tuple(elems) => {
                let ty = self.get_basic_type(&expr.t);
                let mut post_initialization = vec![];
                let values = elems.iter().enumerate().map(|(i, e)| {
                    let (expr, post) = self.compound_constant_types_aux(e, i);
                    if post.is_empty() {
                        expr
                    } else {
                        post_initialization.extend(post.into_iter().map(|(mut inds, v)| {
                            inds.push(ind);
                            (inds, v)
                        }));
                        // Return a zero constant value and save the value to be post initialized
                        expr.get_type().const_zero()
                    }
                }).collect::<Vec<BasicValueEnum<'ctx>>>();
                if ty.is_array_type() {
                    let inner_ty = ty.into_array_type().get_element_type();
                    (match inner_ty {
                        BasicTypeEnum::IntType(t) => t.const_array(&values.into_iter().map(|v| v.into_int_value()).collect::<Vec<IntValue>>()).into(),
                        BasicTypeEnum::FloatType(t) => t.const_array(&values.into_iter().map(|v| v.into_float_value()).collect::<Vec<FloatValue>>()).into(),
                        BasicTypeEnum::PointerType(t) => t.const_array(&values.into_iter().map(|v| v.into_pointer_value()).collect::<Vec<PointerValue>>()).into(),
                        BasicTypeEnum::ArrayType(t) => t.const_array(&values.into_iter().map(|v| v.into_array_value()).collect::<Vec<ArrayValue>>()).into(),
                        _ => panic!(),
                    }, post_initialization)
                } else if ty.is_struct_type() {
                    (ty.into_struct_type().const_named_struct(&values).into(), post_initialization)
                } else { unreachable!() }
            },
            _ => {
                let expr = self.compile_expr(expr);
                let is_const = match expr {
                    BasicValueEnum::IntValue(v) => v.is_const(),
                    BasicValueEnum::FloatValue(v) => v.is_const(),
                    BasicValueEnum::PointerValue(v) => v.is_const(),
                    _ => unreachable!("Not implemented"),
                };
                (expr, if !is_const { vec![(vec![ind], expr)]  } else { vec![] })
            }
        }
    }
    fn compound_constant_types(&self, expr: &Node) -> BasicValueEnum<'ctx> {
        let (val, post_initialization) = self.compound_constant_types_aux(expr, 0);
        let ty = val.get_type();
        let pnt = self.builder.build_alloca(ty, "alloc_compound_val").unwrap();
        self.builder.build_store(pnt, val).unwrap();
        for (loc, post_val) in post_initialization {
            let tmp = unsafe {
                self.builder.build_in_bounds_gep(
                ty, pnt,
                &loc.iter().rev().map(|i| self.ctx.i32_type().const_int(*i as u64, false)).collect::<Vec<IntValue<'ctx>>>(),
                "access_pos_to_initialize").unwrap()
            };
            self.builder.build_store(tmp, post_val).unwrap();
        }
        pnt.into()
    }

    pub fn compile(&mut self, obj_file_name: &str) {
        let main_func = self.module.add_function(obj_file_name, self.ctx.i32_type().fn_type(&[], false), None);
        let main_block = self.ctx.append_basic_block(main_func, "entry");
        self.builder.position_at_end(main_block);

        for sttm in self.ast {
            self.compile_sttm(sttm);
        }
        // End of main
        self.builder.build_return(Some(&self.ctx.i32_type().const_zero())).unwrap();
        std::fs::write("test.ll", self.module.to_string()).unwrap();
    }

    fn compile_func(&mut self, name: &str, func: &Node) {

        match &*func.v {
            expr @ ASTNode::Func { .. } | expr @ ASTNode::FnCall { .. } => {
                let func_scope = self.peek_def().get_scp();
                let v = func_scope.scp_as_var();
                let captured_vars = if let ScopeAttr::FuncScope { captured_vars, .. } = &func_scope.attrs {
                    captured_vars.clone()
                } else { unreachable!() };

                let function_type = self.get_func_type(&v.t, captured_vars.len());
                let function = self.module.add_function(name, function_type, None);
                // NOTE: It does not matter add the function definition before setting its
                // properties, the changes will affect it too
                self.add_def(name, DefType::Fn(function));

                for (i, p) in function.get_params().iter().enumerate() {
                    if i < captured_vars.len() {
                        let param_name = captured_vars[i].v.t.get_id_name().unwrap();
                        p.set_name(param_name);
                        self.defs.get_mut(param_name).unwrap().push(DefType::Var(p.into_pointer_value()))
                    } else {
                        let param = self.peek_def().get_var().clone();
                        let param_name = param.v.t.get_id_name().unwrap();
                        p.set_name(&param_name);
                        self.add_def(&param_name, DefType::Const(*p));
                    }
                }

                let previous_block = self.builder.get_insert_block();
                let entry_block = self.ctx.append_basic_block(function, "entry");
                // Set the position of the builder at the end of entry_block of the function
                self.builder.position_at_end(entry_block);

                // Build the body of the function, it can be built from a partial application
                match expr {
                    ASTNode::FnCall { caller, params, .. } => {
                        let caller_func = self.get_def(caller.t.get_id_name().unwrap()).get_fn().clone();

                        let args_params: Vec<BasicMetadataValueEnum<'ctx>> = function.get_params().iter().map(|p| p.clone().into()).collect();
                        let parameters = args_params.into_iter().chain(params.iter().map(|p| self.compile_expr(p).into())).collect::<Vec<BasicMetadataValueEnum<'ctx>>>();
                        let return_value = self.builder.build_call(caller_func, &parameters, "ret-call").unwrap().try_as_basic_value();

                        if return_value.is_left() {
                            let l = return_value.left();
                            self.builder.build_return(Some(l.as_ref().unwrap())).unwrap();
                        } else {
                            self.builder.build_return(None).unwrap();
                        }
                    },
                    ASTNode::Func { body, .. } => {
                        for sttm in body {
                            self.compile_sttm(sttm);
                        }
                    },
                    _ => unreachable!()
                }

                // Cleaning the values of variables after compiling the scope, and goes one scope up
                self.clear_cur_scp_vars();
                // Return build to its previous position
                self.builder.position_at_end(previous_block.unwrap_or(entry_block));
            },
            _ => unreachable!(),
        }
    }

    fn compile_sttm(&mut self, sttm: &Node) {
        match &*sttm.v {
            ASTNode::Assign { var: (is_var, tk, _), expr } => {
                let var_name = tk.t.get_id_name().unwrap();
                if let ExprType::FnType(_) = expr.t {
                    self.compile_func(var_name, expr);
                } else {
                    let e = self.compile_expr(expr);
                    if !*is_var {
                        if let Some(elem) = self.find_def(var_name) {
                            let is_var = if let Elem::Var(Var { is_var, .. }) = elem { *is_var } else { false };
                            if is_var {
                                let def = self.get_def(var_name).get_var().clone();
                                self.builder.build_store(def, e).unwrap();
                            } else {
                                self.add_def(var_name, DefType::Const(e));
                            }
                        } else {
                            self.add_def(var_name, DefType::Const(e));
                        }
                    } else {
                        // Creating a variable, alocate space and store
                        let def = self.peek_def();
                        let variable = def.get_var().clone();
                        let v_type = self.get_basic_type(&variable.t);
                        let v_name = variable.v.t.get_id_name().unwrap();
                        let pnt = self.builder.build_alloca(v_type, v_name).unwrap();
                        self.add_def(var_name, DefType::Var(pnt));
                        self.builder.build_store(pnt, e).unwrap();
                    }
                }
            },
            ASTNode::Conditional { .. } => {
                let cur_block = self.builder.get_insert_block().unwrap();
                let end_cond = self.ctx.insert_basic_block_after(cur_block, "end_branch");
                self.compile_conditional(&end_cond, sttm);
                self.builder.position_at_end(end_cond);
            },
            ASTNode::Loop { .. } => {
                let cur_block = self.builder.get_insert_block().unwrap();
                let end_cond = self.ctx.insert_basic_block_after(cur_block, "end_loop");
                self.compile_loop(&end_cond, sttm);
                self.builder.position_at_end(end_cond);
            }
            ASTNode::FnCall { caller, params, is_sttm } => {
                assert!(*is_sttm);
                self.compile_fn_call(caller.t.get_id_name().unwrap(), params);
            },
            ASTNode::FlowChange(tk, expr) =>  {
                match tk {
                    TokenType::Back => {
                        let e : Option<Box<dyn BasicValue>> = expr.as_ref().map(|e| {
                            let expr = self.compile_expr(e);
                            Box::new(expr) as Box<dyn BasicValue>
                        });
                        self.builder.build_return(e.as_deref()).unwrap();
                    },
                    _ => unreachable!(),
                }
            },
            ASTNode::Extern(defs) => {
                let scp = self.cur_scp;
                for (_, tk, _) in defs {
                    let fn_name = tk.t.get_id_name().unwrap();
                    let func = self.find_def(fn_name).unwrap().scp_as_var();
                    let func_type = self.get_func_type(&func.t, 0);
                    let function = self.module.add_function(fn_name, func_type, None);
                    self.add_def(tk.t.get_id_name().unwrap(), DefType::Fn(function));
                    // It's not a function with body, undo the added scope
                    self.cur_ind.pop();
                    self.cur_scp = scp;
                }
            }
            _ => unreachable!()
        }
    }

    fn compile_conditional(&mut self, end_block: &BasicBlock<'ctx>, cond: &Node) {
        if let ASTNode::Conditional { cond, body, next } = &*cond.v {

            if let Some(expr) =  cond.as_ref() {
                let before_branch = self.builder.get_insert_block().unwrap();
                let then_branch = self.ctx.insert_basic_block_after(before_branch, "if");
                let else_branch = self.ctx.insert_basic_block_after(then_branch, "else");
                let val = self.compile_expr(expr).into_int_value();
                self.builder.build_conditional_branch(val, then_branch, else_branch).unwrap();
                self.builder.position_at_end(then_branch);
                for sttm in body {
                    self.compile_sttm(sttm);
                }
                self.builder.build_unconditional_branch(*end_block).unwrap();
                self.builder.position_at_end(else_branch);
                if let Some(n) = next {
                    self.compile_conditional(end_block, n);
                }

            } else {
                for sttm in body {
                    self.compile_sttm(sttm);
                }
                self.builder.build_unconditional_branch(*end_block).unwrap();
            }
        } else { unreachable!() }
    }

    fn compile_loop(&mut self, end_block: &BasicBlock<'ctx>, _loop: &Node) {

        if let ASTNode::Loop { cond, body } = &*_loop.v {
            let before_branch = self.builder.get_insert_block().unwrap();
            let cond_branch = self.ctx.insert_basic_block_after(before_branch, "loop_cond");
            let body_branch = self.ctx.insert_basic_block_after(cond_branch, "loop_body");
            self.builder.build_unconditional_branch(cond_branch).unwrap();
            self.builder.position_at_end(cond_branch);
            let e = self.compile_expr(cond.as_ref().expect("loop still in development"));
            self.builder.build_conditional_branch(e.into_int_value(), body_branch, *end_block).unwrap();
            self.builder.position_at_end(body_branch);
            for sttm in body {
                self.compile_sttm(sttm);
            }
            self.builder.build_unconditional_branch(cond_branch).unwrap();

        } else { unreachable!() }

    }

    fn compile_expr(&self, expr: &Node) -> BasicValueEnum<'ctx> {
        match &*expr.v {
            // ASTNode::Func { args, ret, body } => self.compile_func(expr),
            ASTNode::Binary { .. } => self.compile_bin_op(expr),
            ASTNode::FnCall { caller, params, .. } =>{
                let expr = self.compile_fn_call(caller.t.get_id_name().unwrap(), params);
                expr.try_as_basic_value().left_or_else(|_| panic!("Expected expr to be not None"))
            },
            ASTNode::Leaf(l) => {
                match &l.t {
                    // TODO: change false to proper create a integer that is signed
                    TokenType::Integer(num) => self.get_basic_type(&expr.t).into_int_type().const_int(num.parse::<u64>().unwrap(), false).into(),
                    TokenType::Real(num) => self.get_basic_type(&expr.t).into_float_type().const_float(num.parse::<f64>().unwrap()).into(),
                    TokenType::Str(s) => {
                        let mut llvm_str: Vec<u8> = vec![];
                        let mut found_scape = false;
                        for c in s.bytes() {
                            if found_scape {
                                llvm_str.push(match c {
                                    b'n' => 10,
                                    b'r' => 13,
                                    b't' => 9,
                                    b'0' => 0,
                                    _ => panic!("Scape char not implemented")
                                });
                                found_scape = false;
                            } else {
                                if c == b'\\' { found_scape = true; continue }
                                llvm_str.push(c);
                            }
                        }
                        self.builder.build_global_string_ptr(&llvm_str.into_iter().map(|b| b as char).collect::<String>(), ".str").unwrap().as_basic_value_enum()
                    },
                    TokenType::Id(name) =>
                        match self.get_def(name) {
                            DefType::Var(pnt) => {
                                let v = self.find_def(name).unwrap().get_var();
                                let v_type = self.get_basic_type(&v.t);
                                self.builder.build_load(v_type, *pnt, "tmpload").unwrap()
                            },
                            DefType::Const(constant) => constant.clone(),
                            _ => unreachable!()
                        },
                    TokenType::StruAccess(name) => {
                        let mut words = name.split('.');
                        let name_var = words.next().unwrap();
                        let pnt = self.get_def(name_var).get_const().into_pointer_value();
                        let ty = if let ScopeAttr::TupleScope { t, .. } = &self.find_def(name_var).unwrap().get_scp().attrs {
                            self.get_basic_type(t)
                        } else { unreachable!() };
                        let indexes = vec![self.ctx.i32_type().const_zero()] // First index is 0, to access the pointer
                                        .into_iter()
                                        .chain(words.into_iter().map(|w| {
                                            if let Ok(num) = w.parse::<u32>() {
                                                self.ctx.i32_type().const_int(num as u64, false)
                                            } else {
                                                todo!("Not implemented.")
                                            }
                        })).collect::<Vec<IntValue>>();
                        let field_pnt = unsafe { self.builder.build_in_bounds_gep(ty, pnt, &indexes, "tuple_access").unwrap() };
                        let field_ty = self.get_basic_type(&expr.t);
                        self.builder.build_load(field_ty, field_pnt, "tuple_access_val").unwrap()
                    },
                    _ => unreachable!("Expression Leaf not implemented: {:?}", l.t),
                }
            },
            ASTNode::Cast { e, t } => {
                let e = self.compile_expr(e);
                let cur_ty = e.get_type();
                let new_ty = self.get_basic_type(t);
                use {BasicTypeEnum::*, inkwell::values::InstructionOpcode};
                let op = match (cur_ty, new_ty) {
                    (IntType(t1), IntType(t2)) if t1.get_bit_width() > t2.get_bit_width() => InstructionOpcode::Trunc,
                    (IntType(t1), IntType(t2)) if t1.get_bit_width() < t2.get_bit_width() => InstructionOpcode::ZExt,
                    _ => todo!("Conversion between {cur_ty} and {new_ty} not implemented"),
                };
                self.builder.build_cast(op, e, new_ty, "cast").unwrap()
            },
            ASTNode::Deref { e, i, .. } => {
                let ty = self.get_basic_type(&expr.t);
                let var = self.compile_expr(e).into_pointer_value();
                let ind = if let Some(i) = i {
                    self.compile_expr(i).into_int_value()
                } else { self.ctx.i64_type().const_int(0, false) };
                let elem_pnt = unsafe {
                    self.builder.build_in_bounds_gep(ty, var, &[ind], "get_elem_at").unwrap()
                };
                if ty.is_array_type() {
                    elem_pnt.into()
                } else {
                    self.builder.build_load(ty, elem_pnt, "get_elem_val").unwrap()
                }
            },
            ASTNode::Tuple(_) | ASTNode::Array(_) => self.compound_constant_types(expr),
            _ => {
                unreachable!("compile_expr: Not implemented {:?}", *expr.v)
            },
        }
    }

    fn compile_bin_op(&self, bin: &Node) -> BasicValueEnum<'ctx> {
        use ExprType::*;
        let (op, lhs, rhs) = if let ASTNode::Binary { op, l, r } = &*bin.v {
            (op, self.compile_expr(l), self.compile_expr(r))
        } else { unreachable!() };
        let bld = &self.builder;

        match op.t {
            TokenType::Add => {
                match &bin.t {
                    Int { .. } | Char => bld.build_int_add(lhs.into_int_value(), rhs.into_int_value(), "addtmp").unwrap().into(),
                    Real(_) => bld.build_float_add(lhs.into_float_value(), rhs.into_float_value(), "faddtmp").unwrap().into(),
                    _ => unreachable!("Not implemented"),
                }
            },
            TokenType::Sub => {
                match &bin.t {
                    Int { .. } | Char => bld.build_int_sub(lhs.into_int_value(), rhs.into_int_value(), "subtmp").unwrap().into(),
                    Real(_) => bld.build_float_sub(lhs.into_float_value(), rhs.into_float_value(), "fsubtmp").unwrap().into(),
                    _ => unreachable!("Not implemented"),
                }
            },
            TokenType::Mul => {
                match &bin.t {
                    Int { .. } | Char => bld.build_int_mul(lhs.into_int_value(), rhs.into_int_value(), "multmp").unwrap().into(),
                    Real(_) => bld.build_float_mul(lhs.into_float_value(), rhs.into_float_value(), "fmultmp").unwrap().into(),
                    _ => unreachable!("Not implemented"),
                }
            },
            TokenType::Div => {
                match &bin.t {
                    // TODO: refactor to check if use or not the unsigned div
                    Int { .. } | Char => bld.build_int_unsigned_div(lhs.into_int_value(), rhs.into_int_value(), "divtmp").unwrap().into(),
                    Real(_) => bld.build_float_div(lhs.into_float_value(), rhs.into_float_value(), "fdivtmp").unwrap().into(),
                    _ => unreachable!("Not implemented"),
                }
            },
            TokenType::Eq | TokenType::LeT | TokenType::LeE | TokenType::GrT | TokenType::GrE | TokenType::Neq => {
                use TokenType::{Neq, Eq, LeE, LeT, GrE, GrT};
                self.builder.build_int_compare(match &op.t {
                    Neq => IntPredicate::NE, Eq => IntPredicate::EQ,
                    LeT => IntPredicate::ULT, LeE => IntPredicate::ULE,
                    GrT => IntPredicate::UGT, GrE => IntPredicate::UGE,
                    _ => panic!("Comparison not implemented")
                     // TODO: Verify cast lhs and rhs into flot or into int
                }, lhs.into_int_value(), rhs.into_int_value(), "cmp").unwrap().into()
            },
            _ => unreachable!("Not implemented")
        }
    }

    fn compile_fn_call(&self, name: &str, params: &Vec<Node>) -> CallSiteValue<'ctx> {
        let func = self.get_def(name).get_fn();
        let func_params = func.get_params();
        let captured_var_names = func_params.iter().take(func_params.len() - params.len()).map(|p| p.get_name()).collect::<Vec<&CStr>>();
        let parameters = captured_var_names.into_iter().map(|n| {
            if let DefType::Var(pnt) = self.get_def(n.to_str().unwrap()) {
                pnt.as_basic_value_enum().into()
            } else { unreachable!() }
        }).chain(
            params.iter().map(|p| self.compile_expr(p).into())
        ).collect::<Vec<BasicMetadataValueEnum<'ctx>>>();
        self.builder.build_call(*func, &parameters, "tmpcall").unwrap()
    }

    fn get_basic_type_metadata(&self, t: &ExprType) -> BasicMetadataTypeEnum<'ctx> {
        self.get_basic_type(t).into()
    }

    fn get_basic_type(&self, t: &ExprType) -> BasicTypeEnum<'ctx> {
        match self.get_type(t) {
            AnyTypeEnum::IntType(t) => t.into(),
            AnyTypeEnum::FloatType(t) => t.into(),
            AnyTypeEnum::PointerType(t) => t.into(),
            AnyTypeEnum::StructType(t) => t.into(),
            AnyTypeEnum::ArrayType(t) => t.into(),
            _ => panic!("The passed type is not a Basic Type")
        }
    }

    /// Return None when the ExprType is Void, as Void does not implements BasicType trait
    fn get_type(&self, t: &ExprType) -> AnyTypeEnum<'ctx> {
        let context = self.ctx;
        match t {
            ExprType::Int { bits, .. } => context.custom_width_int_type(*bits as u32).into(),
            ExprType::Real(bits) => context.f64_type().into(), // TODO: use bits to return the correct type
            ExprType::Char => context.i8_type().into(),
            ExprType::Bool => context.custom_width_int_type(1).into(),
            ExprType::None => context.void_type().into(),
            ExprType::TupleType(inner) => context.struct_type(&inner.iter().map(|t| self.get_basic_type(t)).collect::<Vec<BasicTypeEnum<'ctx>>>(), false).into(),
            ExprType::UnionType(inner) => context.struct_type(&inner.iter().map(|t| self.get_basic_type(t)).collect::<Vec<BasicTypeEnum<'ctx>>>(), false).into(),
            ExprType::Array(inner, len) => self.get_basic_type(inner).array_type(*len as u32).into(),
            ExprType::Pnt(_) | ExprType::PntVar(_)  => context.ptr_type(inkwell::AddressSpace::default()).into(),
            _ => panic!("get_type: Match {t:?} not implemented"),
        }
    }

    pub fn test(&self) {
        // Create a new LLVM context and module
        let context = Context::create();
        let module = context.create_module("printf_example");
        let builder = context.create_builder();

        // Declare the `printf` function
        let i32_type = context.i32_type();
        let ptr_type = context.ptr_type(inkwell::AddressSpace::default());
        let printf_type = i32_type.fn_type(&[ptr_type.into()], true);
        let printf = module.add_function("printf", printf_type, None);

        // Define the `main` function
        let main_type = i32_type.fn_type(&[], false);
        let main_func = module.add_function("main", main_type, None);
        let entry_block = context.append_basic_block(main_func, "entry");
        builder.position_at_end(entry_block);

        // Create a format string
        let hello_world = builder.build_global_string_ptr("Hello, generated printf!\n", ".str").unwrap();

        // Call `printf` with the format string
        builder.build_call(printf, &[hello_world.as_pointer_value().into()], "").unwrap();

        // Return 0 from `main`
        builder.build_return(Some(&i32_type.const_int(0, false))).unwrap();

        // Print the generated LLVM IR
        // module.print_to_stderr();
        // Write the LLVM IR to a file
        let ir_file_path = "output.ll";
        let mut file = std::fs::File::create(ir_file_path).expect("Failed to create IR file");
        let llvm_ir = module.to_string();
        file.write_all(llvm_ir.as_bytes())
            .expect("Failed to write IR to file");

        // Optionally, compile and execute the module
        // let engine = module
        //     .create_jit_execution_engine(OptimizationLevel::None)
        //     .unwrap();
        // unsafe {
        //     engine.run_function(main_func, &[]);
        // }
    }

}
