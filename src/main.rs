mod grammar;
mod tokenizer;

fn main() -> Result<(), Box<dyn std::error::Error>>{


    let mut t = tokenizer::Tokenizer::new("src/text.txt")?;
    while let Some(tk) = t.get_next_token() {
        println!("{:?}", tk);
    }

    Ok(())
}

