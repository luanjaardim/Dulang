#include "grammar.h"
#include "tokenizer.h"

void Grammar::loadGrammar(Grammar *gm) {
  TokenizedFile *tf = readToTokenizedFile(this->filePath.c_str());
  gm->extractPatterns(tf);
}

Element *getNextElement(TokenizedFile *tf) {

  if( //if the pattern is: name:
    currToken(*tf) && currToken(*tf)->type == TK_NAME && 
    peekLineToken(*tf, 1) && peekLineToken(*tf, 1)->type == TK_COLON
  ) {
    nextToken(tf, 1);
    return new Element(ElementKey(peekBackLineToken(*tf, 1)->text));
  }
  if( //if the pattern is: <name>
    currToken(*tf) && currToken(*tf)->type == TK_LOG_LT &&
    peekLineToken(*tf, 1) && peekLineToken(*tf, 1)->type == TK_NAME &&
    peekLineToken(*tf, 2) && peekLineToken(*tf, 2)->type == TK_LOG_GT
  ) {
    nextToken(tf, 2);
    return new Element(InnerElement(peekBackLineToken(*tf, 1)->text));
  }
  if( //if the pattern is: "name"
    currToken(*tf) && currToken(*tf)->type == TK_STR
  ) {
    return new Element(ElementText(currToken(*tf)->text.substr(1, currToken(*tf)->text.size()-2)));
  }
  if(
    currToken(*tf) && currToken(*tf)->type == TK_NAME
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
    if(e && e->type == INNER_ELEMENT && nextToken(tf, 1) && currToken(*tf)->type == TK_COLON) {
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
      printf("Grammar Error at: %d %d\n", (int)currToken(*tf)->l, (int)currToken(*tf)->c);
      exit(1);
    }
  }
  if(currToken(*tf) && currToken(*tf)->type == TK_QUEST) { // if the pattern is: ?( elems... )
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

Token *getTokenIfBeforeAndAdvance(TokenizedFile *tf, Position end) {
  if(tf->pos.isAfter(end)) {
    return NULL;
  }
  Token *tk = currToken(*tf);
  nextToken(tf, 1);
  // TODO: check if the token is in the same line as the end
  // if(tf->pos.sameLine(end))
  //   nextLineToken(tf, 1);
  // else
  //   nextToken(tf, 1);
  return tk;
}

bool handleElementType(
  TokenizedFile *tf, 
  Pattern p,
  Element *e,
  PatternSteps *steps,
  size_t elem_idx, 
  Position *end
);

/*
* Returns the index of the next element, if it fails, returns 0
* If there is no next element, returns the size of the line
*/
Position findStartOfNextElement(
  TokenizedFile *tf,
  Pattern p,
  Element *nextElem,
  PatternSteps *steps,
  size_t nextElemIdx,
  Position *end
) {
  if(nextElemIdx >= p.elements.size()) return *end;
  if(nextElem->type == VALUE && (nextElem->value.type == VAL_NEW_LINE || nextElem->value.type == VAL_BLOCK)) {
    Position start = tf->pos;
    size_t indent = getLineIndentation(tf->lines[tf->pos.l]);
    while(advanceLineTokenizdFile(tf) && tf->pos.isBefore(*end)) {
      if(getLineIndentation(tf->lines[tf->pos.l]) <= indent) {
        Position tmp = tf->pos;
        tf->pos.goToPos(start);
        return Position(tmp.l-1, tf->lines[tmp.l-1]->tokens.size());
      }
    }
    tf->pos.goToPos(start);
    return Position();
  }

  TokenizedFile *copy = cloneTokenizedFile(*tf);
  Position currElem = tf->pos;
  bool failed = true;

  do {
    if(handleElementType(
      copy, p, nextElem, steps, nextElemIdx, end
    )) {
      failed = false;
      break;
    } else copy->pos.goToPos(currElem);
    currElem.e++;
  } while(nextLineToken(copy, 1) && currElem.isBefore(*end));
  delete copy;
  if(failed) return Position();
  return currElem;
}

bool handleElementType(
  TokenizedFile *tf,
  Pattern p,
  Element *e,
  PatternSteps *steps,
  size_t elem_idx, 
  Position *end
) {
  // WARNING: experimental if, it can cause some code not being parsed, remove it and test again if it's not working
  if((e->type == TEXT || (e->type == VALUE && e->value.type != VAL_BLOCK && e->value.type != VAL_NEW_LINE)) &&
    tf->pos.sameLine(*end) && end->e - tf->pos.e > 1 && elem_idx == p.elements.size() - 1) {
    //this if tries to avoid tokens that should not exist
    return false;
  }
  if(tf->pos.isAfter(*end)) return false; //if the end of the pattern was reached
  
  if(e->type == TEXT) {
    Token *tk = getTokenIfBeforeAndAdvance(tf, *end);
    if(!tk || e->text.text != tk->text) return false;
  } else if(e->type == INNER_ELEMENT) {

    PatternSteps tmp = PatternSteps(steps->parentPatternKey);
    Position endOfCurrent = findStartOfNextElement(tf, p, p.elements[elem_idx+1], &tmp, elem_idx+1, end);
    if(endOfCurrent.found == false) return false;
    steps->steps.push_back( 
      PatternStep(e->innerElement.elem, tf->pos, endOfCurrent)
    );
    tf->pos.goToPos(endOfCurrent);

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
    Position endOfCurrent = findStartOfNextElement(tf, p, p.elements[idx+1], &tmp, idx+1, end);
    if(endOfCurrent.found == false) return false;
    if(endOfCurrent.equals(tf->pos)) return true;

    Position start = tf->pos;
    for(auto elem : e->optionalElement.elements) {
      if(!handleElementType(tf, p, elem, steps, elem_idx, end)) {
        Element *nextElem = p.elements[elem_idx+1];
        if(nextElem->type == OPTIONAL) {
          tf->pos.goToPos(start);
          break; //the next optional can take what was not accepted here
        }
        return false;
      }
    }

  } else if(e->type == LIST) {

    PatternSteps tmp = PatternSteps(steps->parentPatternKey);
    Position nextElementStart = findStartOfNextElement(tf, p, p.elements[elem_idx+1], &tmp, elem_idx+1, end);
    if(nextElementStart.found == false) return false;

    Position sep_pos = {};
    while(tf->pos.isBefore(nextElementStart)) { //iterate over the list
      sep_pos = findStartOfNextElement(tf, p, e->list.separator, &tmp, elem_idx, end);
      // TODO: verifies func_def args with a comma without anything after
      if(sep_pos.found == false) {
        steps->steps.push_back(
          PatternStep(e->list.e->innerElement.elem, tf->pos, nextElementStart)
        );
        tf->pos.goToPos(nextElementStart);
        return true;
      }
      Position lastPos = nextElementStart.e > 0 ? Position(nextElementStart.l, nextElementStart.e-1) : nextElementStart;
      if(sep_pos.equals(lastPos) || sep_pos.isAfter(lastPos)) {
        break;
      }
      steps->steps.push_back(
        PatternStep(e->list.e->innerElement.elem, tf->pos, sep_pos)
      );
      tf->pos.goToPos(sep_pos);
      //consumes separator
      if(!handleElementType(tf, p, e->list.separator, steps, elem_idx, end)) return false;
    }

  } else if(e->type == VALUE){
    switch(e->value.type) {
      case VAL_NAME:
        if(getTokenIfBeforeAndAdvance(tf, *end)->type != TK_NAME) return false;
        break;
      case VAL_NUMBER:
        if(getTokenIfBeforeAndAdvance(tf, *end)->type != TK_INT) return false;
        break;
      case VAL_STRING:
        if(getTokenIfBeforeAndAdvance(tf, *end)->type != TK_STR) return false;
        break;
      case VAL_CHAR:
        if(getTokenIfBeforeAndAdvance(tf, *end)->type != TK_CHAR) return false;
        break;
      case VAL_BLOCK:
      case VAL_NEW_LINE:
        if(tf->pos.e < tf->lines[tf->pos.l]->tokens.size() - 1)
          return false;
        advanceLineTokenizdFile(tf);
        break;
      case VAL_INDENT:
        return false;
        // TODO: find a way to check if the token is an indent
        break;
    }
  } else {
    printf("Type not known\n");
    exit(1);
  }

  return true;
}

Node<Token *> *Grammar::parseFile(TokenizedFile *tf, PatternStep ps) {
  vector<Pattern> patterns = this->patterns[ps.key];
  vector<PatternSteps> possibleSteps;
  PatternSteps bestSteps = PatternSteps(ps.key), tmpSteps = PatternSteps(ps.key);
  size_t pat_idx = 0, elem_idx = 0;
  Position end = ps.end;
  bool failed = false;

  for( Pattern p : patterns ) {
    elem_idx = 0;
    tf->pos.goToPos(ps.start);
    tmpSteps.steps.clear();
    tmpSteps.patternIdx = pat_idx;

    for( Element *e : p.elements ) {
      if((failed = !handleElementType(tf, p, e, &tmpSteps, elem_idx, &end))) {
        tmpSteps.steps.clear();
        break;
      }
      elem_idx++;
    }

    if(!failed) {
      if(tmpSteps.steps.size() == bestSteps.steps.size()) {
        for(int i = 0; i < (int)bestSteps.steps.size(); i++) {
          if(bestSteps.steps[i].end.isBefore(tmpSteps.steps[i].end)) {
            bestSteps = tmpSteps;
            possibleSteps.clear();
            break;
          }
        }
        possibleSteps.push_back(tmpSteps);
      } else if(tmpSteps.steps.size() > bestSteps.steps.size()) {
        bestSteps = tmpSteps;
        possibleSteps.clear();
        possibleSteps.push_back(bestSteps);
      }
    }

    pat_idx++;
  }
  //choose the steps to take and make the recursion for the inner elements
  for( auto steps : possibleSteps ) {

    Node<Token *> *answer = new Node<Token *>(NULL), *first = answer;
    tf->pos.goToPos(ps.start);
    size_t curIntervalIdx = 0;
    while(tf->pos.isBefore(ps.end)) {
      // when the currToken is NULL, it means we are the end of that line
      // this happens when the token identified is a NEW_LINE
      if(currToken(*tf) == NULL) advanceLineTokenizdFile(tf);

      if(curIntervalIdx < steps.steps.size()) {
        if(tf->pos.equals(steps.steps[curIntervalIdx].start)) {

          auto child = this->parseFile(tf, steps.steps[curIntervalIdx]);
          if(!child) { answer = NULL; break; }
          if(!child->get_data()) {
            for(int i = CHILD(1); i < (int)child->get_neighbors_size(); i++) {
              if(child->get_neighbor(i) == NULL) continue;
              Node<Token *> *tmp = child->get_neighbor(i);
              child->unlink(tmp);
              Node<Token *>::linkFatherAndChild(answer, tmp);
            }
            delete child;
          } else Node<Token *>::linkFatherAndChild(answer, child);

          tf->pos.goToPos(steps.steps[curIntervalIdx].end);
          curIntervalIdx++;
          continue;
        }
      }
      if(answer->get_data() == NULL)
        answer->set_data(currToken(*tf));
      else {
        Node<Token *> *newNode = new Node<Token *>(NULL);
        Node<Token *>::linkNodeNextTo(answer, newNode);
        newNode->set_data(currToken(*tf));
        answer = newNode;
      }
      if(nextToken(tf, 1) == NULL) break;
    }
    if(answer) {
      return first;
    } //else delete first; TODO: free memory
  }

  // print current steps
  // for(auto step : bestSteps.steps) {
  //   printf("Key: %s\n", step.key.c_str());
  //   printf("Line: %d\n", (int)step.start.l);
  //   printf("Start: %d\n", (int)step.start.e);
  //   printf("End: %d\n", (int)step.end.e);
  // }

  return NULL;
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

//traversing the AST and printing the tokens
void Grammar::printAST(Node<Token *> *node, string tab) {
  if(node == NULL) return;
  printf("%sToken: %s\n", tab.c_str(), node->get_data() ? node->get_data()->text.c_str() : "NULL");
  for(int i = CHILD(1); i < (int)node->get_neighbors_size(); i++) {
    if(node->get_neighbor(i) == NULL) continue;
    string child_tab = tab + "  ";
    printf("%s%dth child:\n", child_tab.c_str(), i - RIGHT_LINK);
    printAST(node->get_neighbor(i), child_tab + "  ");
  }
  printAST(node->get_neighbor(RIGHT_LINK), tab);
}
