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
    nextToken(tf, 1);
    return new Element(ElementKey(peekBackLineToken(*tf, 1)->text));
  }
  if( //if the pattern is: <name>
    currToken(*tf) && currToken(*tf)->type == CMP_LT &&
    peekLineToken(*tf, 1) && peekLineToken(*tf, 1)->type == NAME_TK &&
    peekLineToken(*tf, 2) && peekLineToken(*tf, 2)->type == CMP_GT
  ) {
    nextToken(tf, 2);
    return new Element(InnerElement(peekBackLineToken(*tf, 1)->text));
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
  if(currToken(*tf) && currToken(*tf)->text == "[") { // if the pattern is: [ elem : "separator"]
    // printf("Separator\n");
    nextToken(tf, 1);
    Element *e = getNextElement(tf);
    if(e && nextToken(tf, 1) && currToken(*tf)->type == COLON) {
      nextToken(tf, 1);
      Element *s = getNextElement(tf); //separator
      if(s && nextToken(tf, 1) && currToken(*tf)->text == "]") {
        return new Element(ElementList(e, s));
      }
      else {
        printf("Grammar Error at line, list separator: %d\n", (int)currToken(*tf)->l);
        exit(1);
      }
    }
    else {
      printf("Grammar Error at line: %d\n", (int)currToken(*tf)->l);
      exit(1);
    }
  }
  if(currToken(*tf) && currToken(*tf)->type == QUESTION_TK) { // if the pattern is: ?( elems... )
    nextLineToken(tf, 1); //skip the "?"
    OptionalElement e = OptionalElement();
    while(nextLineToken(tf, 1) && currToken(*tf)->text != ")") {
      Element *inner_elem = getNextElement(tf);
      if(inner_elem) {
        e.elements.push_back(inner_elem);
      }
      else {
        printf("Grammar Error, optional value, at line: %d\n", (int)currToken(*tf)->l);
        exit(1);
      }
    }
    return new Element(e);
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
        curKey = e->key.key;
        this->patterns[curKey] = {Pattern(curId++)};
        // printf("Current key: %s\n", curKey.c_str());
        appendPattern = false;
        delete e;
        continue; // skip the appendPattern = true
      }
      if(appendPattern) {
        this->patterns[curKey].push_back(Pattern(curId++));
        appendPattern = false;
      }
      this->patterns[curKey].back().elements.push_back(e);
    }

    if(!peekLineToken(*tf, 1)) { //check if the next token is at new line
      appendPattern = true;
    }
  } while(nextToken(tf, 1));
  printf("End of file\n");
}

void printType(Element *e, size_t tab) {
  printf("%*s", (int)tab, "");
  if(e->type == KEY) {
    printf("Key: %s\n", e->key.key.c_str());
  }
  else if(e->type == VALUE) {
    vector<string> humanReadable = { "NUMBER", "CHAR", "STRING", "NAME", "INDENT", "DEDENT", "NEW_LINE" };
    printf("Value: %s\n", humanReadable[(int)e->value.type].c_str());
  }
  else if(e->type == TEXT) {
    printf("Text: %s\n", e->text.text.c_str());
  }
  else if(e->type == INNER_ELEMENT) {
    printf("Inner element: %s\n", e->innerElement.elem.c_str());
  }
  else if(e->type == OPTIONAL) {
    printf("Optional\n");
    for(auto elem : e->optionalElement.elements) {
      printType(elem, tab + 2);
    }
  }
  else if(e->type == LIST) {
    printf("Element to repeat\n");
    printType(e->list.e, tab + 2);
    printf("%*s", (int)tab, "");
    printf("Separator\n");
    printType(e->list.separator, tab + 2);
  }
}

void Grammar::printPatterns() {
  vector<string> humanReadable = { "KEY", "VALUE", "TEXT", "INNER_ELEMENT", "OPTIONAL", "LIST" };
  for( auto key : this->patterns ) {
    printf("Key: %s\n", key.first.c_str());
    for( auto pattern : key.second ) {
      printf("\tPattern id: %d\n", (int)pattern.id);
      for( auto elem : pattern.elements ) {
        printf("\t\tElement type: %s\n", humanReadable[(int)elem->type].c_str());
        printType(elem, 20);
      }
    }
  }
}
