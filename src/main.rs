mod grammar;
mod tokenizer;

fn main() -> Result<(), Box<dyn std::error::Error>>{

    let p = grammar::Parser::new("src/text.txt")?;
    println!("{:#?}", p.parse().unwrap());

    Ok(())
}

