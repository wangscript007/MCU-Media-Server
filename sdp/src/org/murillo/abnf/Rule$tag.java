/* -----------------------------------------------------------------------------
 * Rule$tag.java
 * -----------------------------------------------------------------------------
 *
 * Producer : com.parse2.aparse.Parser 2.2
 * Produced : Mon Mar 23 16:37:55 CET 2015
 *
 * -----------------------------------------------------------------------------
 */

package org.murillo.abnf;

import java.util.ArrayList;

final public class Rule$tag extends Rule
{
  private Rule$tag(String spelling, ArrayList<Rule> rules)
  {
    super(spelling, rules);
  }

  public Object accept(Visitor visitor)
  {
    return visitor.visit(this);
  }

  public static Rule$tag parse(ParserContext context)
  {
    context.push("tag");

    boolean parsed = true;
    int s0 = context.index;
    ArrayList<Rule> e0 = new ArrayList<Rule>();
    Rule rule;

    parsed = false;
    if (!parsed)
    {
      {
        ArrayList<Rule> e1 = new ArrayList<Rule>();
        int s1 = context.index;
        parsed = true;
        if (parsed)
        {
          boolean f1 = true;
          int c1 = 0;
          for (int i1 = 0; i1 < 1 && f1; i1++)
          {
            rule = Rule$DIGIT.parse(context);
            if ((f1 = rule != null))
            {
              e1.add(rule);
              c1++;
            }
          }
          for (int i1 = 1; i1 < 9 && f1; i1++)
          {
            rule = Rule$DIGIT.parse(context);
            if ((f1 = rule != null))
            {
              e1.add(rule);
              c1++;
            }
          }
          parsed = c1 >= 1;
        }
        if (parsed)
          e0.addAll(e1);
        else
          context.index = s1;
      }
    }

    rule = null;
    if (parsed)
      rule = new Rule$tag(context.text.substring(s0, context.index), e0);
    else
      context.index = s0;

    context.pop("tag", parsed);

    return (Rule$tag)rule;
  }
}

/* -----------------------------------------------------------------------------
 * eof
 * -----------------------------------------------------------------------------
 */
