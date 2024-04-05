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
    return new Element(ElementText(currToken(*tf)->text.substr(1, currToken(*tf)->text.size()-2)));
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
    if(text == "BLOCK")
      return new Element(ElementValue(VAL_BLOCK));
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
        printf("Grammar Error at line %d, list separator: %d\n", (int)currToken(*tf)->l, (int)currToken(*tf)->c);
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

Token *getTokenIfBeforeAndAdvance(TokenizedFile *tf, size_t end) {
  if(tf->currElem >= end) {
    return NULL;
  }
  Token *tk = currToken(*tf);
  nextLineToken(tf, 1);
  return tk;
}

bool handleElementType(
  TokenizedFile *tf, 
  Pattern p,
  Element *e,
  PatternSteps *steps,
  size_t elem_idx, 
  size_t *end
);

/*
* Returns the index of the next element, if it fails, returns 0
* If there is no next element, returns the size of the line
*/
int findStartOfNextElement(
  TokenizedFile *tf,
  Pattern p,
  Element *nextElem,
  PatternSteps *steps,
  size_t nextElemIdx,
  size_t *end
) {
  TokenizedFile *copy = cloneTokenizedFile(*tf);
  size_t currElem = tf->currElem;
  bool failed = true;
  if(nextElemIdx >= p.elements.size()) return tf->lines[tf->currLine]->tokens.size();

  do {
    if(handleElementType(
      copy, p, nextElem, steps, nextElemIdx, end // WARNING: maybe pass this end as ref cause bugs
    )) {
      failed = false;
      break;
    } else copy->currElem = currElem;
    currElem++;
  } while(nextLineToken(copy, 1));
  delete copy;
  if(failed) return -1;
  return currElem;
}

bool handleElementType(
  TokenizedFile *tf,
  Pattern p,
  Element *e,
  PatternSteps *steps,
  size_t elem_idx, 
  size_t *end
) {
  if(e->type == TEXT) {
    Token *tk = getTokenIfBeforeAndAdvance(tf, *end);
    if(!tk || e->text.text != tk->text) return false;
  } else if(e->type == INNER_ELEMENT) {

    PatternSteps tmp = PatternSteps(steps->parentPatternKey);
    int endOfCurrent = findStartOfNextElement(tf, p, p.elements[elem_idx+1], &tmp, elem_idx+1, end);
    if(endOfCurrent == -1) return false;
    steps->steps.push_back( 
      PatternStep(e->innerElement.elem, tf->currLine, tf->currElem, endOfCurrent)
    );
    tf->currElem = endOfCurrent;

  } else if(e->type == OPTIONAL) {

    PatternSteps tmp = PatternSteps(steps->parentPatternKey);
    size_t idx = elem_idx;
    for(int ith_elem = idx + 1; ith_elem < (int)p.elements.size(); ith_elem++ ) {
      //find the element right behind the one that is not optional
      if(p.elements[ith_elem]->type != OPTIONAL) {
        idx = ith_elem - 1;
        break;
      }
    }
    int endOfCurrent = findStartOfNextElement(tf, p, p.elements[idx+1], &tmp, idx+1, end);
    if(endOfCurrent == -1) return false;
    if(endOfCurrent == (int)tf->lines[tf->currLine]->tokens.size()) return true;

    size_t start = tf->currElem;
    if((int) tf->currElem != endOfCurrent) {
      for(auto elem : e->optionalElement.elements) {
        if(!handleElementType(tf, p, elem, steps, elem_idx, end)) {
          Element *nextElem = p.elements[elem_idx+1];
          if(nextElem->type == OPTIONAL) {
            tf->currElem = start;
            break; //the next optional can take what was not accepted here
          }
          return false;
        }
      }
    }

  } else if(e->type == LIST) {

    PatternSteps tmp = PatternSteps(steps->parentPatternKey);
    int nextElementStart = findStartOfNextElement(tf, p, p.elements[elem_idx+1], &tmp, elem_idx+1, end);
    if(nextElementStart == -1) return false;

    int sep_idx = 0;
    while((int) tf->currElem < nextElementStart) { //iterate over the list
      if((sep_idx = findStartOfNextElement(tf, p, e->list.separator, &tmp, elem_idx, end)) >= nextElementStart - 1) {
        break;
      }
      if(sep_idx == -1) {
        steps->steps.push_back(
          PatternStep(e->list.e->innerElement.elem, tf->currLine, tf->currElem, nextElementStart)
        );
        tf->currElem = nextElementStart;
        return true;
      }
      steps->steps.push_back(
        PatternStep(e->list.e->innerElement.elem, tf->currLine, tf->currElem, sep_idx)
      );
      tf->currElem = sep_idx;
      if(!handleElementType(tf, p, e->list.separator, steps, elem_idx, end)) return false;
    }

  } else if(e->type == VALUE){
    Token *tk = getTokenIfBeforeAndAdvance(tf, *end);
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
        *end = tf->lines[tf->currLine]->tokens.size();
        break;
      case VAL_INDENT:
        if(tf->lines[tf->currLine]->tokens[0]->c <= tf->lines[tf->currLine-1]->tokens[0]->c) return false;
        for(int i = tf->currLine + 1; i < (int)tf->lines.size(); i++) {
          //verifies if the identation is respected, if a line after the current has a smaller identation and
          //is diferent of the previous line, it's an error
          if(tf->lines[i]->tokens[0]->c < tf->lines[tf->currLine]->tokens[0]->c &&
            tf->lines[i]->tokens[0]->c != tf->lines[tf->currLine-1]->tokens[0]->c)
              return false;
        }
        break;
      case VAL_BLOCK:
        //iterate over lines till find a line that has the same identation, if EOF is find, return false
        size_t c = tf->lines[tf->currLine]->tokens[0]->c;
        do 
          if(advanceLineTokenizdFile(tf) == 0) return false; 
        while(tf->lines[tf->currLine]->tokens[0]->c != c);
        *end = tf->lines[tf->currLine]->tokens.size();
        break;
    }
  } else {
    printf("Type not known\n");
    exit(1);
  }

  return true;
}

bool Grammar::parseFile(TokenizedFile *tf, PatternStep ps) {
  vector<Pattern> patterns = this->patterns[ps.key];
  PatternSteps bestSteps = PatternSteps(ps.key), tmpSteps = PatternSteps(ps.key);
  size_t pat_idx = 0, elem_idx = 0, end = ps.end;

  for( Pattern p : patterns ) {
    elem_idx = 0;
    tf->currLine = ps.line;
    tf->currElem = ps.start;

    for( Element *e : p.elements ) {
      tmpSteps.patternIdx = pat_idx;
      if(!handleElementType(tf, p, e, &tmpSteps, elem_idx, &end)) {
        tmpSteps.steps.clear();
        break;
      }
      elem_idx++;
    }

    if(tmpSteps.steps.size() >= bestSteps.steps.size()) {
      if(tmpSteps.steps.size() == bestSteps.steps.size()) {
        for(int i = 0; i < (int)bestSteps.steps.size(); i++) {
          if(bestSteps.steps[i].end - bestSteps.steps[i].start >
             tmpSteps.steps[i].end - tmpSteps.steps[i].start) {
            bestSteps = tmpSteps;
            break;
          }
        }
      } else bestSteps = tmpSteps;
    }

    pat_idx++;
  }
  //choose the steps to take and make the recursion for the inner elements

  //print current steps
  for(auto step : bestSteps.steps) {
    printf("Key: %s\n", step.key.c_str());
    printf("Line: %d\n", (int)step.line);
    printf("Start: %d\n", (int)step.start);
    printf("End: %d\n", (int)step.end);
  }

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
