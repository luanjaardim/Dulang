mod grammar;
mod tokenizer;

fn main() -> Result<(), Box<dyn std::error::Error>>{


    let t = tokenizer::Tokenizer::new("src/text.txt")?;
    let p = grammar::Parser::new(t);
    println!("{:#?}", p.parse().unwrap());

    Ok(())
}

