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

#include "ast_optimizer.hh"
#include <algorithm>
#include <vector>

using namespace std;
using namespace ast;

// -----------------------------------------------------------------------
// Node equality check (structural, for matching loop body candidates)
// -----------------------------------------------------------------------

static int nodesEqual(ASTNode *a, ASTNode *b)
{
  SequenceNode *sa, *sb;
  ChoiceNode *ca, *cb;
  LoopNode *la, *lb;
  size_t i;

  if(a->kind != b->kind)
    return 0;

  switch(a->kind)
    {
    case ASTKind::Terminal:
      return static_cast<TerminalNode*>(a)->text ==
             static_cast<TerminalNode*>(b)->text;
    case ASTKind::Nonterminal:
      return static_cast<NonterminalNode*>(a)->name ==
             static_cast<NonterminalNode*>(b)->name;
    case ASTKind::Epsilon:
    case ASTKind::Newline:
      return 1;
    case ASTKind::Sequence:
      sa = static_cast<SequenceNode*>(a);
      sb = static_cast<SequenceNode*>(b);
      if(sa->children.size() != sb->children.size())
        return 0;
      for(i = 0; i < sa->children.size(); i++)
        {
          if(!nodesEqual(sa->children[i], sb->children[i]))
            return 0;
        }
      return 1;
    case ASTKind::Choice:
      ca = static_cast<ChoiceNode*>(a);
      cb = static_cast<ChoiceNode*>(b);
      if(ca->alternatives.size() != cb->alternatives.size())
        return 0;
      for(i = 0; i < ca->alternatives.size(); i++)
        {
          if(!nodesEqual(ca->alternatives[i], cb->alternatives[i]))
            return 0;
        }
      return 1;
    case ASTKind::Optional:
      return nodesEqual(static_cast<OptionalNode*>(a)->child,
                        static_cast<OptionalNode*>(b)->child);
    case ASTKind::Loop:
      la = static_cast<LoopNode*>(a);
      lb = static_cast<LoopNode*>(b);
      if(la->repeats.size() != lb->repeats.size())
        return 0;
      if((la->body == NULL) != (lb->body == NULL))
        return 0;
      if(la->body != NULL && !nodesEqual(la->body, lb->body))
        return 0;
      for(i = 0; i < la->repeats.size(); i++)
        {
          if(!nodesEqual(la->repeats[i], lb->repeats[i]))
            return 0;
        }
      return 1;
    }
  return 0;
}

// -----------------------------------------------------------------------
// mergeSequences: flatten nested Sequence nodes
// -----------------------------------------------------------------------

static int mergeSequences(ASTNode *n)
{
  int count = 0;
  size_t i, j;
  SequenceNode *seq, *inner;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;

  switch(n->kind)
    {
    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      // Recurse first
      for(i = 0; i < seq->children.size(); i++)
        count += mergeSequences(seq->children[i]);
      // Flatten nested sequences
      i = 0;
      while(i < seq->children.size())
        {
          if(seq->children[i]->kind == ASTKind::Sequence)
            {
              inner = static_cast<SequenceNode*>(seq->children[i]);
              seq->children.erase(seq->children.begin() + i);
              for(j = 0; j < inner->children.size(); j++)
                seq->children.insert(seq->children.begin() + i + j,
                                     inner->children[j]);
              i += inner->children.size();
              inner->children.clear();
              delete inner;
              count++;
            }
          else
            {
              i++;
            }
        }
      break;
    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        count += mergeSequences(ch->alternatives[i]);
      break;
    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      count += mergeSequences(opt->child);
      break;
    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        count += mergeSequences(loop->body);
      for(i = 0; i < loop->repeats.size(); i++)
        count += mergeSequences(loop->repeats[i]);
      break;
    default:
      break;
    }
  return count;
}

// -----------------------------------------------------------------------
// mergeChoices: flatten nested Choice nodes, expand Optional in Choice
// -----------------------------------------------------------------------

static int mergeChoices(ASTNode *n)
{
  int count = 0;
  size_t i, j;
  ChoiceNode *ch, *inner;
  SequenceNode *seq;
  OptionalNode *opt;
  LoopNode *loop;

  switch(n->kind)
    {
    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      // Recurse first
      for(i = 0; i < ch->alternatives.size(); i++)
        count += mergeChoices(ch->alternatives[i]);
      // Flatten nested choices
      i = 0;
      while(i < ch->alternatives.size())
        {
          if(ch->alternatives[i]->kind == ASTKind::Choice)
            {
              inner = static_cast<ChoiceNode*>(ch->alternatives[i]);
              ch->alternatives.erase(ch->alternatives.begin() + i);
              for(j = 0; j < inner->alternatives.size(); j++)
                ch->alternatives.insert(ch->alternatives.begin() + i + j,
                                        inner->alternatives[j]);
              i += inner->alternatives.size();
              inner->alternatives.clear();
              delete inner;
              count++;
            }
          else if(ch->alternatives[i]->kind == ASTKind::Optional)
            {
              // Expand Optional inside Choice:
              // Choice([..., Optional(X), ...])
              //   -> Choice([..., Epsilon, X, ...])
              // Also handle Optional(Choice([a,b]))
              //   -> Choice([..., Epsilon, a, b, ...])
              opt = static_cast<OptionalNode*>(ch->alternatives[i]);
              ch->alternatives.erase(ch->alternatives.begin() + i);
              ch->alternatives.insert(ch->alternatives.begin() + i,
                                      new EpsilonNode());
              i++;
              if(opt->child->kind == ASTKind::Choice)
                {
                  inner = static_cast<ChoiceNode*>(opt->child);
                  for(j = 0; j < inner->alternatives.size(); j++)
                    ch->alternatives.insert(
                      ch->alternatives.begin() + i + j,
                      inner->alternatives[j]);
                  i += inner->alternatives.size();
                  inner->alternatives.clear();
                  opt->child = NULL;
                  delete opt;
                }
              else
                {
                  ch->alternatives.insert(
                    ch->alternatives.begin() + i, opt->child);
                  i++;
                  opt->child = NULL;
                  delete opt;
                }
              count++;
            }
          else
            {
              i++;
            }
        }
      break;
    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        count += mergeChoices(seq->children[i]);
      break;
    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      count += mergeChoices(opt->child);
      break;
    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        count += mergeChoices(loop->body);
      for(i = 0; i < loop->repeats.size(); i++)
        count += mergeChoices(loop->repeats[i]);
      break;
    default:
      break;
    }
  return count;
}

// -----------------------------------------------------------------------
// liftWrappers: remove single-child Sequence/Choice wrappers
// Returns the (possibly replaced) node.
// The caller is responsible for updating the pointer.
// -----------------------------------------------------------------------

static ASTNode* liftWrappers(ASTNode *n, int *count)
{
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;
  ASTNode *child;
  size_t i;

  switch(n->kind)
    {
    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        seq->children[i] = liftWrappers(seq->children[i], count);
      if(seq->children.size() == 1)
        {
          child = seq->children[0];
          seq->children.clear();
          delete seq;
          (*count)++;
          return child;
        }
      return n;
    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        ch->alternatives[i] = liftWrappers(ch->alternatives[i], count);
      if(ch->alternatives.size() == 1)
        {
          child = ch->alternatives[0];
          ch->alternatives.clear();
          delete ch;
          (*count)++;
          return child;
        }
      return n;
    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      opt->child = liftWrappers(opt->child, count);
      // Optional(Loop(body=null, ...)) == Loop(body=null, ...)
      // Both mean zero-or-more, so unwrap the Optional.
      if(opt->child->kind == ASTKind::Loop)
        {
          loop = static_cast<LoopNode*>(opt->child);
          if(loop->body == NULL)
            {
              opt->child = NULL;
              delete opt;
              (*count)++;
              return (ASTNode*)loop;
            }
        }
      // Optional(Choice(a, b, c)) -> Choice(Epsilon, a, b, c)
      if(opt->child->kind == ASTKind::Choice)
        {
          ch = static_cast<ChoiceNode*>(opt->child);
          ch->alternatives.insert(ch->alternatives.begin(),
                                  new EpsilonNode());
          opt->child = NULL;
          delete opt;
          (*count)++;
          return (ASTNode*)ch;
        }
      return n;
    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        loop->body = liftWrappers(loop->body, count);
      for(i = 0; i < loop->repeats.size(); i++)
        loop->repeats[i] = liftWrappers(loop->repeats[i], count);
      return n;
    default:
      return n;
    }
}

// -----------------------------------------------------------------------
// countMatches: count how many TRAILING elements of a repeat
// alternative match trailing elements of the parent sequence before
// position 'before'.
//
// Repeats in the AST are in forward (unreversed) order.  In the
// railroad diagram they run right-to-left, so the last element of
// the repeat is what appears first in the feedback path.  The old
// graph optimizer works on already-reversed repeats and checks
// leading elements.  Here we check trailing elements instead.
//
// For a Sequence repeat, compare children from the end backwards
// against parentChildren[before], parentChildren[before-1], ...
// For a single node, compare directly (match count 0 or 1).
// -----------------------------------------------------------------------

static int countMatches(ASTNode *repeat,
                        vector<ASTNode*> &parentChildren,
                        int before)
{
  int matchCount = 0;
  int pi;
  int ci;
  SequenceNode *seq;

  if(repeat->kind == ASTKind::Sequence)
    {
      seq = static_cast<SequenceNode*>(repeat);
      pi = before;
      ci = (int)seq->children.size() - 1;
      while(ci >= 0 && pi >= 0)
        {
          if(nodesEqual(seq->children[ci], parentChildren[pi]))
            {
              matchCount++;
              ci--;
              pi--;
            }
          else
            {
              return matchCount;
            }
        }
    }
  else
    {
      if(before >= 0 && nodesEqual(repeat, parentChildren[before]))
        matchCount = 1;
    }
  return matchCount;
}

// -----------------------------------------------------------------------
// stripTrailing: strip minMatch trailing elements from a repeat
// alternative (since we match trailing elements, not leading).
// Returns the (possibly simplified) node.
// -----------------------------------------------------------------------

static ASTNode* stripTrailing(ASTNode *repeat, int minMatch)
{
  SequenceNode *seq;
  ASTNode *remaining;
  int ci;

  if(repeat->kind == ASTKind::Sequence)
    {
      seq = static_cast<SequenceNode*>(repeat);
      for(ci = 0; ci < minMatch; ci++)
        {
          delete seq->children.back();
          seq->children.pop_back();
        }
      if(seq->children.size() == 1)
        {
          remaining = seq->children[0];
          seq->children.clear();
          delete seq;
          return remaining;
        }
      if(seq->children.size() == 0)
        {
          delete seq;
          return new EpsilonNode();
        }
      return seq;
    }
  else
    {
      // Single node matched entirely
      delete repeat;
      return new EpsilonNode();
    }
}

// -----------------------------------------------------------------------
// analyzeLoops: detect and extract loop bodies from Sequence context
//
// Non-optional pattern:
//   Sequence([..., X, Loop(body=null, repeats=[...])])
//   where each repeat's leading elements (reversed) match X etc.
//
// Optional pattern:
//   Sequence([..., X, Optional(Loop(body=null, repeats=[...]))])
//   Same matching, plus unwrap the Optional.
//
// Standalone Optional(Loop(null,...)) is simplified to Loop(null,...).
// -----------------------------------------------------------------------

static int analyzeLoopsInSequence(SequenceNode *seq);

static int analyzeLoops(ASTNode *n)
{
  int count = 0;
  size_t i;
  SequenceNode *seq;
  ChoiceNode *ch;
  OptionalNode *opt;
  LoopNode *loop;

  switch(n->kind)
    {
    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      // Recurse into children first
      for(i = 0; i < seq->children.size(); i++)
        count += analyzeLoops(seq->children[i]);
      count += analyzeLoopsInSequence(seq);
      break;
    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        count += analyzeLoops(ch->alternatives[i]);
      break;
    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      count += analyzeLoops(opt->child);
      break;
    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL)
        count += analyzeLoops(loop->body);
      for(i = 0; i < loop->repeats.size(); i++)
        count += analyzeLoops(loop->repeats[i]);
      break;
    default:
      break;
    }
  return count;
}

static int analyzeLoopsInSequence(SequenceNode *seq)
{
  int count = 0;
  size_t ri;
  LoopNode *loop;
  OptionalNode *optWrap;
  int numAlts, ai;
  int minMatch, allZero, anyZero;
  int *altMatches;
  ASTNode *newBody;
  int bi, di;
  int isOptional;

  ri = 0;
  while(ri < seq->children.size())
    {
      loop = NULL;
      isOptional = 0;

      // Check for Loop(body=null, ...) at position ri
      if(seq->children[ri]->kind == ASTKind::Loop)
        {
          loop = static_cast<LoopNode*>(seq->children[ri]);
          if(loop->body != NULL)
            {
              loop = NULL;  // body already set, skip
            }
        }
      // Check for Optional(Loop(body=null, ...)) at position ri
      else if(seq->children[ri]->kind == ASTKind::Optional)
        {
          optWrap = static_cast<OptionalNode*>(seq->children[ri]);
          if(optWrap->child->kind == ASTKind::Loop)
            {
              loop = static_cast<LoopNode*>(optWrap->child);
              if(loop->body != NULL)
                loop = NULL;
              else
                isOptional = 1;
            }
        }

      if(loop == NULL)
        {
          ri++;
        }
      else
        {
          numAlts = loop->repeats.size();

          // Compute match counts
          altMatches = new int[numAlts];
          minMatch = -1;
          allZero = 1;
          anyZero = 0;
          for(ai = 0; ai < numAlts; ai++)
            {
              altMatches[ai] = countMatches(loop->repeats[ai],
                                            seq->children,
                                            (int)ri - 1);
              if(altMatches[ai] > 0)
                {
                  allZero = 0;
                  if(minMatch < 0 || altMatches[ai] < minMatch)
                    minMatch = altMatches[ai];
                }
              else
                {
                  anyZero = 1;
                }
            }

          // For non-optional loops, ALL alternatives must match
          // For optional loops, we can still extract if some match
          if(!isOptional && (allZero || anyZero || minMatch <= 0))
            {
              delete[] altMatches;
              ri++;
            }
          else if(isOptional && (allZero || minMatch <= 0))
            {
              // No match, but still unwrap Optional(Loop(null,...))
              // -> Loop(null,...) since they're semantically the same
              delete[] altMatches;
              optWrap = static_cast<OptionalNode*>(seq->children[ri]);
              seq->children[ri] = optWrap->child;
              optWrap->child = NULL;
              delete optWrap;
              count++;
              // Don't advance ri — re-check in case of cascaded patterns
            }
          else
            {
              // Build new body from matched parent elements
              if(minMatch == 1)
                {
                  newBody = seq->children[ri - 1]->clone();
                }
              else
                {
                  SequenceNode *bodySeq = new SequenceNode();
                  for(bi = minMatch; bi > 0; bi--)
                    bodySeq->append(
                      seq->children[ri - bi]->clone());
                  newBody = bodySeq;
                }

              // Strip matched leading elements from each repeat
              for(ai = 0; ai < numAlts; ai++)
                {
                  if(altMatches[ai] > 0)
                    loop->repeats[ai] =
                      stripTrailing(loop->repeats[ai], minMatch);
                }

              delete[] altMatches;

              // Move Epsilon repeats to the end
              stable_partition(loop->repeats.begin(),
                               loop->repeats.end(),
                               [](ASTNode *n)
                               { return n->kind != ASTKind::Epsilon; });

              // Set the loop body
              loop->body = newBody;

              // Remove matched elements from parent sequence
              for(di = 0; di < minMatch; di++)
                {
                  delete seq->children[ri - minMatch];
                  seq->children.erase(
                    seq->children.begin() + (ri - minMatch));
                }
              ri -= minMatch;

              // If this was Optional(Loop), unwrap it
              if(isOptional)
                {
                  optWrap =
                    static_cast<OptionalNode*>(seq->children[ri]);
                  seq->children[ri] = loop;
                  optWrap->child = NULL;
                  delete optWrap;
                }

              count++;
              // Don't advance ri — re-check
            }
        }
    }

  return count;
}

// -----------------------------------------------------------------------
// flattenLoopChoices: for loops, if a repeat child is a Choice,
// flatten its alternatives into direct repeats.  Also remove Epsilon
// repeats when non-Epsilon alternatives exist.
// -----------------------------------------------------------------------

static int flattenLoopChoices(ASTNode *n)
{
  int count = 0;
  size_t i, j;
  LoopNode *loop;
  ChoiceNode *ch;
  SequenceNode *seq;
  OptionalNode *opt;
  int hasNonEpsilon;

  switch(n->kind)
    {
    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      // Recurse into body and repeats
      if(loop->body != NULL)
        count += flattenLoopChoices(loop->body);
      for(i = 0; i < loop->repeats.size(); i++)
        count += flattenLoopChoices(loop->repeats[i]);
      // Flatten Choice repeats
      i = 0;
      while(i < loop->repeats.size())
        {
          if(loop->repeats[i]->kind == ASTKind::Choice)
            {
              ch = static_cast<ChoiceNode*>(loop->repeats[i]);
              loop->repeats.erase(loop->repeats.begin() + i);
              for(j = 0; j < ch->alternatives.size(); j++)
                loop->repeats.insert(loop->repeats.begin() + i + j,
                                     ch->alternatives[j]);
              i += ch->alternatives.size();
              ch->alternatives.clear();
              delete ch;
              count++;
            }
          else
            {
              i++;
            }
        }
      // Remove Epsilon repeats when non-Epsilon exists
      hasNonEpsilon = 0;
      for(i = 0; i < loop->repeats.size(); i++)
        {
          if(loop->repeats[i]->kind != ASTKind::Epsilon)
            hasNonEpsilon = 1;
        }
      if(hasNonEpsilon)
        {
          i = 0;
          while(i < loop->repeats.size())
            {
              if(loop->repeats[i]->kind == ASTKind::Epsilon)
                {
                  delete loop->repeats[i];
                  loop->repeats.erase(loop->repeats.begin() + i);
                  count++;
                }
              else
                {
                  i++;
                }
            }
        }
      break;
    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        count += flattenLoopChoices(seq->children[i]);
      break;
    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      for(i = 0; i < ch->alternatives.size(); i++)
        count += flattenLoopChoices(ch->alternatives[i]);
      break;
    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      count += flattenLoopChoices(opt->child);
      break;
    default:
      break;
    }
  return count;
}

// -----------------------------------------------------------------------
// bodyChoiceToChoiceloop: when a loop body is a Choice and the only
// repeat is Epsilon, convert to choiceloop form (body=null, repeats
// from old body's alternatives).  This is only done when the loop is
// in an optional context (inside a Choice that has an Epsilon
// alternative, or wrapped in an Optional).
// -----------------------------------------------------------------------

static int bodyChoiceToChoiceloop(ASTNode *n, int inOptionalContext)
{
  int count = 0;
  size_t i;
  LoopNode *loop;
  ChoiceNode *ch, *bodyCh;
  SequenceNode *seq;
  OptionalNode *opt;
  int isOptCtx;

  switch(n->kind)
    {
    case ASTKind::Loop:
      loop = static_cast<LoopNode*>(n);
      if(loop->body != NULL &&
         loop->body->kind == ASTKind::Choice &&
         loop->repeats.size() == 1 &&
         loop->repeats[0]->kind == ASTKind::Epsilon &&
         inOptionalContext)
        {
          bodyCh = static_cast<ChoiceNode*>(loop->body);
          // Delete the Epsilon repeat
          delete loop->repeats[0];
          loop->repeats.clear();
          // Move body choice alternatives to repeats
          for(i = 0; i < bodyCh->alternatives.size(); i++)
            loop->repeats.push_back(bodyCh->alternatives[i]);
          bodyCh->alternatives.clear();
          delete bodyCh;
          loop->body = NULL;
          count++;
        }
      else
        {
          if(loop->body != NULL)
            count += bodyChoiceToChoiceloop(loop->body, 0);
          for(i = 0; i < loop->repeats.size(); i++)
            count += bodyChoiceToChoiceloop(loop->repeats[i], 0);
        }
      break;
    case ASTKind::Choice:
      ch = static_cast<ChoiceNode*>(n);
      // Check if this choice provides an optional context
      // (has an Epsilon alternative)
      isOptCtx = 0;
      for(i = 0; i < ch->alternatives.size(); i++)
        {
          if(ch->alternatives[i]->kind == ASTKind::Epsilon)
            isOptCtx = 1;
        }
      for(i = 0; i < ch->alternatives.size(); i++)
        count += bodyChoiceToChoiceloop(ch->alternatives[i], isOptCtx);
      break;
    case ASTKind::Sequence:
      seq = static_cast<SequenceNode*>(n);
      for(i = 0; i < seq->children.size(); i++)
        count += bodyChoiceToChoiceloop(seq->children[i],
                                        inOptionalContext);
      break;
    case ASTKind::Optional:
      opt = static_cast<OptionalNode*>(n);
      count += bodyChoiceToChoiceloop(opt->child, 1);
      break;
    default:
      break;
    }
  return count;
}

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

ASTNode* ast_optimizer::optimizeBody(ASTNode *body)
{
  int tmp, liftCount;

  // Phase 1: merge and flatten
  do {
    tmp = mergeChoices(body);
  } while(tmp > 0);

  do {
    tmp = mergeSequences(body);
    liftCount = 0;
    body = liftWrappers(body, &liftCount);
    tmp += liftCount;
  } while(tmp > 0);

  // Phase 2: loop analysis with merge/lift interleaved
  do {
    tmp = analyzeLoops(body);
    tmp += mergeSequences(body);
    liftCount = 0;
    body = liftWrappers(body, &liftCount);
    tmp += liftCount;
    tmp += flattenLoopChoices(body);
    tmp += bodyChoiceToChoiceloop(body, 0);
  } while(tmp > 0);

  return body;
}

void ast_optimizer::optimize(ASTGrammar *grammar)
{
  size_t i;

  for(i = 0; i < grammar->productions.size(); i++)
    grammar->productions[i]->body =
      optimizeBody(grammar->productions[i]->body);
}
