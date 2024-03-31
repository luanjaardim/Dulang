#include "grammar.h"
#include "tokenizer.h"
#include "utils.h"

void Grammar::loadGrammar(Grammar *gm) {
  TokenizedFile *tf = readToTokenizedFile(this->filePath.c_str());
  gm->extractPatterns(tf);
}

Element *getNextElement(TokenizedFile *tf) {

  if( //if the pattern is: name:
    currToken(*tf) && currToken(*tf)->type == NAME_TK && 
    peekLineToken(*tf, 1) && peekLineToken(*tf, 1)->type == COLON
  ) {
    return new Element(ElementKey(currToken(*tf)->text));
  }
  if( //if the pattern is: <name>
    currToken(*tf) && currToken(*tf)->type == CMP_LT &&
    peekLineToken(*tf, 1) && peekLineToken(*tf, 1)->type == NAME_TK &&
    peekLineToken(*tf, 2) && peekLineToken(*tf, 2)->type == CMP_GT
  ) {
    return new Element(InnerElement(peekLineToken(*tf, 1)->text));
  }
  if( //if the pattern is: "name"
    currToken(*tf) && currToken(*tf)->type == STR_TK
  ) {
    return new Element(ElementText(currToken(*tf)->text));
  }
  if(
    currToken(*tf) && currToken(*tf)->type == NAME_TK
  ) {
    string text = currToken(*tf)->text;
    if(text == "NAME")
      return new Element(ElementValue(VAL_NAME));
    if(text == "NUMBER")
      return new Element(ElementValue(VAL_NUMBER));
    if(text == "STRING")
      return new Element(ElementValue(VAL_STRING));
    if(text == "CHAR")
      return new Element(ElementValue(VAL_CHAR));
    if(text == "NEW_LINE")
      return new Element(ElementValue(VAL_NEW_LINE));
    if(text == "INDENT")
      return new Element(ElementValue(VAL_INDENT));
    if(text == "DEDENT")
      return new Element(ElementValue(VAL_DEDENT));
  }

  return NULL;
}

void Grammar::extractPatterns(TokenizedFile *tf) {
  Element *e;
  string curKey;
  size_t curId = 0;
  bool appendPattern = false;
  do {
    e = getNextElement(tf);

    if(e) {
      if(e->type == KEY) {
        // printf("Key: %s\n", e->key.key.c_str());
        curKey = e->key.key;
        this->patterns[curKey] = {Pattern(curId++)};
        printf("Current key: %s\n", curKey.c_str());
        appendPattern = false;
        delete e;
        nextToken(tf, 1); // skip the colon
        continue; // skip the appendPattern = true
      }
      if(e->type == INNER_ELEMENT || e->type == TEXT || e->type == VALUE) {
        // printf("Inner: %s\n", e->inner.elem.c_str());
        if(appendPattern) {
          this->patterns[curKey].push_back(Pattern(curId++));
          appendPattern = false;
        }
        this->patterns[curKey].back().elements.push_back(e);
        printf("inserting at: %s, with type %d\n", curKey.c_str(), e->type);
        if(e->type == INNER_ELEMENT) {
          nextLineToken(tf, 2);
        }
      }
    }
    if(currToken(*tf) && currToken(*tf)->text == "[") { // if the pattern is: [ elem : "separator"]
      while(currToken(*tf) && currToken(*tf)->text != "]") {
        nextToken(tf, 1);
      }
      // nextToken(tf, 1);
      // Element *e = getNextElement(tf);
      // if(e && nextToken(tf, 1) && currToken(*tf)->text == ":") {
      //   nextToken(tf, 1);
      //   Element *s = getNextElement(tf);
      //   if(s) {
      //   }
      // }
    }
    if(currToken(*tf) && currToken(*tf)->text == "?") { // if the pattern is: ?( elems... )
      while(currToken(*tf) && currToken(*tf)->text != ")") {
        nextToken(tf, 1);
      }
    }

    if(!peekLineToken(*tf, 1)) { //check if the next token is at new line
      appendPattern = true;
    }
  } while(nextToken(tf, 1));
  printf("End of file\n");
}
