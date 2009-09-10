/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Patrick Pekczynski <pekczynski@ps.uni-sb.de>
 *
 *  Copyright:
 *     Patrick Pekczynski, 2004/2005
 *
 *  Last modified:
 *     $Date$ by $Author$
 *     $Revision$
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __GECODE_INT_GCC_HH__
#define __GECODE_INT_GCC_HH__

#include <gecode/int.hh>

#include <gecode/int/gcc/gccbndsup.hpp>
#include <gecode/int/gcc/graphsup.hpp>
#include <gecode/int/gcc/occur.hpp>

/**
 * \namespace Gecode::Int::GCC
 * \brief Global cardinality propagators
 * \note The global cardinality propagator with fixed cardinalities does not
 *       not support sharing!
 *
 */

namespace Gecode { namespace Int { namespace GCC {

  /**
   * \brief Bounds consistent global cardinality propagator
   * \par [Reference]
   *  The algorithm is taken from: \n
     \verbatim
     @PROCEEDINGS{quimper-efficient,
     title     = {An Efficient Bounds Consistency Algorithm
                  for the Global Cardinality Constraint},
     year      = {2003},
     volume    = {2833},
     address   = {Kinsale, Ireland},
     month     = {September},
     author    = {Claude-Guy Quimper and Peter van Beek
                  and Alejandro L�pez-Ortiz
                  and Alexander Golynski and Sayyed Bashir Sadjad},
     booktitle = {Proceedings of the 9th International Conference
                  on Principles and Practice of
                  Constraint Programming},
     pages     = {600--614},
     url       = {http://ai.uwaterloo.ca/~vanbeek/publications},
     }
     @TECHREPORT{quimper-efficientTR,
     author      = {Claude-Guy Quimper and Peter van Beek
                    and Alejandro L�pez-Ortiz
                    and Alexander Golynski and
                    Sayyed Bashir Sadjad},
     title       = {An Efficient Bounds Consistency Algorithm
                    for the Global Cardinality Constraint,
                    Technical Report},
     institution = {School of Computer Science,
                    University of Waterloo, Waterloo, Canada},
     year        = {2003},
     url         = {http://ai.uwaterloo.ca/~vanbeek/publications},
     }
     \endverbatim
   *
   * This implementation uses the code that is provided
   * by Peter Van Beek:\n
   * http://ai.uwaterloo.ca/~vanbeek/software/software.html
   * The code here has only been slightly modified to fit Gecode
   * (taking idempotent/non-idempotent propagation into account)
   * and uses a more efficient layout of datastructures (keeping the
   * number of different arrays small).
   *
   * The Bnd class is used to post the propagator and BndImp
   * is the actual implementation taking shared variables into account.
   *
   * Requires \code #include <gecode/int/gcc.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Card, bool isView>
  class Bnd{
  public:
    /**
     * \brief Post propagator for views \a x and cardinalities \a k
     *
     * \a all denotes whether the propagator uses all values occuring
     * in the domains of the problem views specified in \a x. Also
     * checks whether \a x and \a k contain shared views.
     */
    static  ExecStatus  post(Space& home,
                             ViewArray<IntView>& x,
                             ViewArray<Card>& k);
  };

  /**
   * \brief Implementation of the bounds consistent
   * global cardinality propagator
   */
  template<class Card, bool isView, bool shared>
  class BndImp : public Propagator {
    friend class Bnd<Card, isView>;
  protected:
    /// Views on which to perform bounds-propagation
    ViewArray<IntView> x;
    /// Views on which to perform value-propagation (subset of \c x)
    ViewArray<IntView> y;
    /// Array containing either fixed cardinalities or CardViews
    ViewArray<Card> k;
    /**
     * \brief  Data structure storing the sum of the views lower bounds
     * Necessary for reasoning about the interval capacities in the
     * propagation algorithm.
     */
    PartialSum<Card> lps;
    /// Data structure storing the sum of the views upper bounds
    PartialSum<Card> ups;
    /**
     * \brief Stores whether cardinalities are all assigned
     *
     * If all cardinalities are assigned the propagation algorithm
     * only has to perform propagation for the upper bounds.
     */
    bool card_fixed;
    /**
     * \brief Stores whether the minium required occurences of
     *        the cardinalities are all zero. If so, we do not need
     *        to perform lower bounds propagation.
     */
    bool skip_lbc;
    /// Constructor for posting
    BndImp(Space& home, ViewArray<IntView>&, ViewArray<Card>&, bool, bool);
    /// Constructor for cloning \a p
    BndImp(Space& home, bool share, BndImp<Card, isView, shared>& p);

    /// Prune cardinality variables with 0 maximum occurrence
    ExecStatus pruneCards(Space& home);

    /**
     * \brief Lower Bounds constraint (LBC) stating
     * \f$ \forall j \in \{0, \dots, |k|-1\}:
     * \#\{i\in\{0, \dots, |x| - 1\} | x_i = card(k_j)\} \geq min(k_j)\f$
     * Hence the lbc constraints the variables such that every value occurs
     * at least as often as specified by its lower cardinality bound.
     * \param home current space
     * \param x  the problem variables
     * \param nb denotes number of unique bounds
     * \param hall contains information about the hall structure of the problem
     *        (cf. HallInfo)
     * \param rank ranking information about the variable bounds (cf. Rank)
     * \param lps partial sum structure for the lower cardinality bounds (cf. PartialSum)
     * \param mu permutation \f$ \mu \f$ such that
     *        \f$ \forall i\in \{0, \dots, |x|-2\}:
     *        max(x_{\mu(i)}) \leq max(x_{\mu(i+1)})\f$
     * \param nu permutation \f$ \nu \f$ such that
     *        \f$ \forall i\in \{0, \dots, |x|-2\}:
     *        min(x_{\mu(i)}) \leq min(x_{\mu(i+1)})\f$
     */
    ExecStatus lbc(Space& home, int& nb, HallInfo hall[], Rank rank[],
                   int mu[], int nu[]);

    /**
     * \brief Upper Bounds constraint (UBC) stating
     * \f$ \forall j \in \{0, \dots, |k|-1\}:
     * \#\{i\in\{0, \dots, |x| - 1\} | x_i = card(k_j)\} \leq max(k_j)\f$
     * Hence the ubc constraints the variables such that no value occurs
     * more often than specified by its upper cardinality bound.
     * \param home current space
     * \param x  the problem variables
     * \param nb denotes number of unique bounds
     * \param hall contains information about the hall structure of the problem
     *        (cf. HallInfo)
     * \param rank ranking information about the variable bounds (cf. Rank)
     * \param ups partial sum structure for the upper cardinality bounds (cf. PartialSum)
     * \param mu permutation \f$ \mu \f$ such that
     *        \f$ \forall i\in \{0, \dots, |x|-2\}:
     *        max(x_{\mu(i)}) \leq max(x_{\mu(i+1)})\f$
     * \param nu permutation \f$ \nu \f$ such that
     *        \f$ \forall i\in \{0, \dots, |x|-2\}:
     *        min(x_{\mu(i)}) \leq min(x_{\mu(i+1)})\f$
     */
    ExecStatus ubc(Space& home, int& nb, HallInfo hall[], Rank rank[],
                   int mu[], int nu[]);
  public:
    /// Destructor
    virtual size_t dispose(Space& home);
    /// Return how much extra memory is allocated by the propagator
    virtual size_t allocated(void) const;
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost funtion returning dynamic low linear
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
  };

  /**
   * \brief Domain consistent global cardinality propagator
   * \par [Reference]
   *  The algorithm is taken from: \n
   * \anchor CardVarNPCompl
   \verbatim
     @PROCEEDINGS{improvedgcc,
     title     = {Improved Algorithms for the
                  Global Cardinality Constraint},
     year      = {2004},
     volume    = {3528},
     address   = {Toronto, Canada},
     month     = {September},
     author    = {Claude-Guy Quimper and Peter van Beek and
                  Alejandro L�pez-Ortiz and Alexander Golynski},
     booktitle = {Proceedings of the 10th International
                  Conference on Principles and Practice of
                  Constraint Programming},
     url       = {http://ai.uwaterloo.ca/~vanbeek/publications},
     }
     \endverbatim
   *
   * Requires \code #include <gecode/int/gcc.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Card, bool isView>
  class Dom : public Propagator {
  protected:
    /// Views on which to perform domain-propagation
    ViewArray<IntView> x;
    /**
     * \brief Views used to channel information between \c x and \c k
     * (\f$ x \subseteq y \f$).
     */
    ViewArray<IntView> y;
    /// Array containing either fixed cardinalities or CardViews
    ViewArray<Card> k;
    /// Propagation is performed on a variable-value graph (used as cache)
    VarValGraph<Card, isView>* vvg;
    /**
     * \brief Stores whether cardinalities are all assigned
     *
     * If all cardinalities are assigned the propagation algorithm
     * only has to perform propagation for the upper bounds.
     */
    bool card_fixed;
    /// Constructor for cloning \a p
    Dom(Space& home, bool share, Dom<Card, isView>& p);
    /// Constructor for posting
    Dom(Space& home, ViewArray<IntView>&, ViewArray<Card>&, bool);

  public:
    /// Destructor including deallocation of variable-value graph
    virtual size_t dispose(Space& home);
    /// Return how much extra memory is allocated by the propagator
    virtual size_t allocated(void) const;
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /**
     * \brief Cost function
     *
     * As the propagation strongly depends on the domain size of the
     * views on which propagation is performed, the propagation costs
     * are computed as follows, where \c d denotes the size of the
     * largest domain of a view in \c x:
     * - low linear ( \f$ d < 6\f$ )
     * - high linear ( \f$ 6 \leq d < \frac{n}{2} \f$ )
     * - low quadratic ( \f$ \frac{n}{2} \leq d < n^2 \f$)
     * - high cubic  ( \f$ n^2 \leq d \f$)
     */
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Post propagator for views \a x and cardinalities \a k
     *
     * \a all denotes whether the propagator uses all values occuring
     * in the domains of the problem views specified in \a x.
     */
    static  ExecStatus  post(Space& home,
                             ViewArray<IntView>& x, ViewArray<Card>& k);
  };

  /**
   * \brief Value consistent global cardinality propagator
   *
   * Requires \code #include <gecode/int/gcc.hh> \endcode
   * \ingroup FuncIntProp
   */
  template<class Card, bool isView>
  class Val : public Propagator {
  protected:
    /// Views on which to perform value-propagation
    ViewArray<IntView> x;
    /// Array containing either fixed cardinalities or CardViews
    ViewArray<Card> k;
    /// Constructor for cloning \a p
    Val(Space& home, bool share, Val<Card, isView>& p );
    /// Constructor for posting
    Val(Space& home, ViewArray<IntView>&, ViewArray<Card>&);

  public:
    /// Destructor
    virtual size_t dispose(Space& home);
    /// Copy propagator during cloning
    virtual Actor* copy(Space& home, bool share);
    /// Cost funtion returning high linear
    virtual PropCost cost(const Space& home, const ModEventDelta& med) const;
    /// Perform propagation
    virtual ExecStatus  propagate(Space& home, const ModEventDelta& med);
    /**
     * \brief Post propagator for views \a x and cardinalities \a k
     *
     * \a all denotes whether the propagator uses all values occuring
     * in the domains of the problem views specified in \a x.
     */
    static  ExecStatus  post(Space& home,
                             ViewArray<IntView>& x, ViewArray<Card>& k);
  };

}}}

#include <gecode/int/gcc/post.hpp>
#include <gecode/int/gcc/ubc.hpp>
#include <gecode/int/gcc/lbc.hpp>
#include <gecode/int/gcc/val.hpp>
#include <gecode/int/gcc/bnd.hpp>
#include <gecode/int/gcc/dom.hpp>

#endif


// STATISTICS: int-prop

