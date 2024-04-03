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
  }
  if(currToken(*tf) && currToken(*tf)->text == "[") { // if the pattern is: [ <elem> : separator_elem ]
    // printf("Separator\n");
    nextToken(tf, 1);
    Element *e = getNextElement(tf);
    if(e && e->type == INNER_ELEMENT && nextToken(tf, 1) && currToken(*tf)->type == COLON) {
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
  bool appendPattern = false;
  do {
    e = getNextElement(tf);

    if(e) {
      if(e->type == KEY) {
        curKey = e->key.key;
        this->patterns[curKey] = {Pattern()};
        // printf("Current key: %s\n", curKey.c_str());
        appendPattern = false;
        delete e;
        continue; // skip the appendPattern = true
      }
      if(appendPattern) {
        this->patterns[curKey].push_back(Pattern());
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

Token *getNextTokenIfBefore(TokenizedFile *tf, size_t end) {
  if(tf->currElem >= end) {
    return NULL;
  }
  nextLineToken(tf, 1); // advance to the next token
  return peekBackLineToken(*tf, 1); //and return the previous token (that was the current)
}

struct handleElemType {
  TokenizedFile *tf;
  Pattern p;
  Element *e;
  PatternSteps *steps;
  size_t elem_idx, pat_idx, *end;
};

bool handleElementType(struct handleElemType h) {
  Token *tk;
  Element *e = h.e;
  TokenizedFile *tf = h.tf;
  Pattern p = h.p;
  PatternSteps *bestSteps = h.steps;

  if(e->type == TEXT) {
    tk = getNextTokenIfBefore(tf, *h.end);
    if(!tk || e->text.text != tk->text) return false;
  } else if(e->type == INNER_ELEMENT) {
    if(h.elem_idx == p.elements.size()-1) //the element will get everything till the end of the line
      bestSteps->steps.push_back( 
        PatternStep(e->innerElement.elem, tf->currLine, tf->currElem, tf->lines[tf->currLine]->tokens.size())
      );
    else {
      Element *nextElem = p.elements[h.elem_idx + 1]; //the next element must be a text or a value
      if(nextElem->type != TEXT && nextElem->type != VALUE) {
        printf("Grammar Error, inner element must be followed by a text or a value, at line: %d\n", (int)currToken(*tf)->l);
        exit(1);
      }
      TokenizedFile *copy = cloneTokenizedFile(*tf);
      // WARNING: Don't know if this works
      while(nextLineToken(copy, 1)) {
        if(handleElementType(handleElemType{
          copy, p, nextElem, bestSteps, h.elem_idx + 1, h.pat_idx, h.end // WARNING: maybe pass this end as ref cause bugs
        })) {
          bestSteps->steps.push_back( 
            PatternStep(e->innerElement.elem, tf->currLine, tf->currElem, copy->currElem)
          );
        }
      }
      delete copy;
      if(!currToken(*tf)) return false;
    }
  } else if(e->type == OPTIONAL) {
    //verifies the next element, if it does not match, check for this element first
    printf("Optional not implemented!!!!\n");
  } else if(e->type == LIST) {
    //check for every inner element till a separator, stops when the separator is not found more
    printf("List not implemented!!!!\n");
  } else if(e->type == VALUE){
    switch(e->value.type) {
      case VAL_NAME:
        if(tk->type != NAME_TK) return false;
        break;
      case VAL_NUMBER:
        if(tk->type != INT_TK) return false;
        break;
      case VAL_STRING:
        if(tk->type != STR_TK) return false;
        break;
      case VAL_CHAR:
        if(tk->type != CHAR_TK) return false;
        break;
      case VAL_NEW_LINE:
        if(advanceLineTokenizdFile(tf) == 0) return false;
        *h.end = tf->lines[tf->currLine]->tokens.size();
        break;
      case VAL_INDENT:
        if(tf->lines[tf->currLine]->tokens[0]->c <= tf->lines[tf->currLine-1]->tokens[0]->c) return false;
        for(int i = tf->currLine + 1; i < tf->lines.size(); i++) {
          //verifies if the identation is respected, if a line after the current has a smaller identation and
          //is diferent of the previous line, it's an error
          if(tf->lines[i]->tokens[0]->c < tf->lines[tf->currLine]->tokens[0]->c &&
            tf->lines[i]->tokens[0]->c != tf->lines[tf->currLine-1]->tokens[0]->c)
              return false;
        }
      case VAL_BLOCK:
        //iterate over lines till find a line that has the same identation, if EOF is find, return false
        size_t c = tf->lines[tf->currLine]->tokens[0]->c;
        do 
          if(advanceLineTokenizdFile(tf) == 0) return false; 
        while(tf->lines[tf->currLine]->tokens[0]->c != c);
        *h.end = tf->lines[tf->currLine]->tokens.size();
        break;
      default:
        printf("Type not known for VALUE element!!!!\n");
    }
  } else {
    printf("Type not known\n");
    exit(1);
  }

  return true;
}

bool Grammar::parseFile(TokenizedFile *tf, PatternStep ps) {
  vector<Pattern> patterns = this->patterns[ps.key];
  PatternSteps bestSteps = PatternSteps(), tmpSteps = PatternSteps();
  Token *tk;
  size_t pat_idx = 0, elem_idx = 0, end = ps.end;

  for( Pattern p : patterns ) {
    elem_idx = 0;
    tf->currLine = ps.line;
    tf->currElem = ps.start;

    for( Element *e : p.elements ) {
      if(!handleElementType(handleElemType{tf, p, e, &tmpSteps, elem_idx, pat_idx, &end})){
        tmpSteps.steps.clear();
        break;
      }
      elem_idx++;
    }
    // TODO: compare tmpSteps with bestSteps
    if(tmpSteps.steps.size() > bestSteps.steps.size()) {
      bestSteps = tmpSteps;
    }

    pat_idx++;
  }
  //choose the steps to take and make the recursion for the inner elements

  return true;
}

void printType(Element *e, size_t tab) {
  printf("%*s", (int)tab, "");
  if(e->type == KEY) {
    printf("Key: %s\n", e->key.key.c_str());
  }
  else if(e->type == VALUE) {
    vector<string> humanReadable = { "NUMBER", "CHAR", "STRING", "NAME", "INDENT", "NEW_LINE", "BLOCK" };
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
      for( auto elem : pattern.elements ) {
        printf("\tElement type: %s\n", humanReadable[(int)elem->type].c_str());
        printType(elem, 16);
      }
    }
  }
}
