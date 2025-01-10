use super::TokenType::{self, *};

impl TokenType {
    fn cmp(&self, other: &Self) -> bool {
        match (self, other) {
            (Str, Str) | (Integer, Integer) | (Real, Real) | (Character, Character) | (Comment, Comment) |
            (Add, Add) | (Sub, Sub) | (Mul, Mul) | (Div, Div) | (Eq, Eq) | (Neq, Neq) | (GrT, GrT) |
            (GrE, GrE) | (LeT, LeT) | (LeE, LeE) | (And, And) | (Or, Or) | (Not, Not) | (Band, Band) | (Bor, Bor) |
            (Bnot, Bnot) | (Shl, Shl) | (Shr, Shr) | (Bxor, Bxor) |
            (If, If) | (Elif, Elif) | (Else, Else) | (While, While) |
            (Loop, Loop) | (Switch, Switch) | (Skip, Skip) | (Stop, Stop) | (Back, Back) |
            (I(_), I(_)) | (U(_), U(_)) | (F(_), F(_)) | (Char, Char) | (Bool, Bool) | (FnType, FnType) |
            (UnionType, UnionType) | (TupleType, TupleType) | (Type, Type) | (Assign, Assign) | (TypeInf, TypeInf) |
            (OpCurly, OpCurly) | (ClCurly, ClCurly) | (OpParen, OpParen) | (ClParen, ClParen) | (Comma, Comma) | (Dot, Dot) |
            (Colon, Colon) | (Semicolon, Semicolon) | (TokenType::None, TokenType::None) | (Nl, Nl) | (Id(_), Id(_)) => true,
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
