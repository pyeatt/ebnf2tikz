/*
ebnf2tikz
    An optimizing compiler to convert (possibly annotated) Extended
    Backus-Naur Form (EBNF) to railroad diagrams expressed as LaTeX
    TikZ commands.

    Copyright (C) 2021  Larry D. Pyeatt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * @file resolver.cc
 * @brief Implementation of reference checking and subsumption.
 *
 * @see resolver.hh for the public interface
 */

#include "resolver.hh"
#include "ast_visitor.hh"
#include <regex.h>
#include <set>
#include <string>
#include <iostream>

using namespace std;
using namespace ast;

// -----------------------------------------------------------------------
// Reference checking
// -----------------------------------------------------------------------

/**
 * @brief Visitor that collects all nonterminal names referenced in an AST.
 */
class RefCollector : public DefaultASTVisitor {
public:
  set<string> &refs;

  RefCollector(set<string> &r) : refs(r) {}

  void visitNonterminal(NonterminalNode *n) override
  {
    refs.insert(n->name);
  }
};

void resolver::checkReferences(ASTGrammar *grammar)
{
  set<string> defined;
  set<string> referenced;
  size_t i;

  // Collect all defined production names
  for(i = 0; i < grammar->productions.size(); i++)
    defined.insert(grammar->productions[i]->name);

  // Collect all referenced nonterminals
  {
    RefCollector collector(referenced);
    for(i = 0; i < grammar->productions.size(); i++)
      grammar->productions[i]->body->accept(collector);
  }

  // Warn about undefined references
  for(auto it = referenced.begin(); it != referenced.end(); it++)
    {
      if(defined.find(*it) == defined.end())
        diagnostics.report(Severity::Warning,
          string("undefined nonterminal '") + *it + "'");
    }
}

// -----------------------------------------------------------------------
// Subsumption
// -----------------------------------------------------------------------

/**
 * @brief Replace matching nonterminal references with clones of a replacement body.
 *
 * Recursively walks the AST.  When a NonterminalNode's name matches
 * the compiled regex, it is replaced with a deep clone of the
 * replacement body.
 *
 * @param n           The node to process (may be replaced).
 * @param pattern     Compiled POSIX extended regex to match against.
 * @param replacement The body to clone when a match is found.
 * @return The resulting node (may be a new clone or the original).
 */
static ASTNode* subsumeNode(ASTNode *n, regex_t *pattern,
                            ASTNode *replacement)
{
  regmatch_t pmatch[1];
  NonterminalNode *nt;
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  ASTNode *result;
  size_t i;

  switch(n->kind)
    {
    case ASTKind::Nonterminal:
      nt = static_cast<NonterminalNode*>(n);
      if(!regexec(pattern, nt->name.c_str(), 1, pmatch, 0))
        return replacement->clone();
      return n;

    case ASTKind::Terminal:
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return n;

    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        {
          result = subsumeNode(seq->children[i], pattern, replacement);
          if(result != seq->children[i])
            {
              delete seq->children[i];
              seq->children[i] = result;
            }
        }
      return n;

    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        {
          result = subsumeNode(ch->alternatives[i], pattern, replacement);
          if(result != ch->alternatives[i])
            {
              delete ch->alternatives[i];
              ch->alternatives[i] = result;
            }
        }
      return n;

    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      result = subsumeNode(opt->child, pattern, replacement);
      if(result != opt->child)
        {
          delete opt->child;
          opt->child = result;
        }
      return n;

    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != nullptr)
        {
          result = subsumeNode(loop->body, pattern, replacement);
          if(result != loop->body)
            {
              delete loop->body;
              loop->body = result;
            }
        }
      for(i = 0; i < loop->repeats.size(); i++)
        {
          result = subsumeNode(loop->repeats[i], pattern, replacement);
          if(result != loop->repeats[i])
            {
              delete loop->repeats[i];
              loop->repeats[i] = result;
            }
        }
      return n;
    }
  return n;
}

void resolver::subsume(ASTGrammar *grammar)
{
  size_t i, j;
  regex_t compiled;
  string reg;
  ASTProduction *prod;

  for(i = 0; i < grammar->productions.size(); i++)
    {
      prod = grammar->productions[i];
      if(prod->annotations == nullptr)
        ;  /* no annotations, skip */
      else
        {
          reg = (*prod->annotations)["subsume"];
          if(reg == "")
            ;  /* no subsume annotation, skip */
          else
            {
              if(reg == "emusbussubsume")
                reg = string("^") + prod->name + string("$");

              if(regcomp(&compiled, reg.c_str(), REG_EXTENDED))
                {
                  diagnostics.report(Severity::Error,
                    string("unable to compile regex '") + reg + "'");
                }
              else
                {
                  for(j = 0; j < grammar->productions.size(); j++)
                    {
                      if(j != i)
                        subsumeNode(grammar->productions[j]->body,
                                    &compiled, prod->body);
                    }
                  regfree(&compiled);
                }
            }
        }
    }
}
