// filename: crates/alncore/src/parser.rs

#[derive(Debug)]
pub struct AlnParseError {
    pub line: usize,
    pub message: String,
}
