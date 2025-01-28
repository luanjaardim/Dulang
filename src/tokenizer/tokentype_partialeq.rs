use super::TokenType::{self, *};

impl TokenType {
    fn cmp(&self, other: &Self) -> bool {
        match (self, other) {
            (Str(_), Str(_)) | (Integer(_), Integer(_)) | (Real(_), Real(_)) | (Character(_), Character(_)) | (Comment, Comment) |
            (StruAccess(_), StruAccess(_)) | (ModAccess(_), ModAccess(_)) |
            (Add, Add) | (Sub, Sub) | (Mul, Mul) | (Div, Div) | (Eq, Eq) | (Neq, Neq) | (GrT, GrT) |
            (GrE, GrE) | (LeT, LeT) | (LeE, LeE) | (And, And) | (Or, Or) | (Not, Not) | (Band, Band) | (Bor, Bor) |
            (Bnot, Bnot) | (Shl, Shl) | (Shr, Shr) | (Bxor, Bxor) |
            (If, If) | (Elif, Elif) | (Else, Else) | (While, While) |
            (Loop, Loop) | (Mod, Mod) | (Skip, Skip) | (Stop, Stop) | (Back, Back) |
            (I(_), I(_)) | (U(_), U(_)) | (F(_), F(_)) | (Char, Char) | (Bool, Bool) | (FnType, FnType) |
            (UnionType, UnionType) | (TupleType, TupleType) | (Type, Type) | (Assign, Assign) | (TypeInf, TypeInf) |
            (Ref, Ref) | (VarRef, VarRef) | (VarDef, VarDef) | (Deref, Deref) | (ClSqrBra, ClSqrBra) | (OpSqrBra, OpSqrBra) |
            (OpCurly, OpCurly) | (ClCurly, ClCurly) | (OpParen, OpParen) | (ClParen, ClParen) | (Comma, Comma) |
            (Colon, Colon) | (Semicolon, Semicolon) | (TokenType::None, TokenType::None) | (Nl, Nl) | (Id(_), Id(_)) |
            (PassL, PassL) | (PassR, PassR) |
            (True, True) | (False, False)
            => true,
            _ => false,
        }
    }
}

impl PartialEq for TokenType {
    fn eq(&self, other: &Self) -> bool {
        self.cmp(other)
    }
    fn ne(&self, other: &Self) -> bool {
        !self.cmp(other)
    }
}
