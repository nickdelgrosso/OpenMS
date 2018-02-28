// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2017.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Hendrik Weisser $
// $Authors: Hendrik Weisser, Lucia Espona $
// --------------------------------------------------------------------------

#ifndef OPENMS_ANALYSIS_ID_IDCONFLICTRESOLVERALGORITHM
#define OPENMS_ANALYSIS_ID_IDCONFLICTRESOLVERALGORITHM

#include <OpenMS/APPLICATIONS/TOPPBase.h>

#include <OpenMS/FORMAT/ConsensusXMLFile.h>
#include <OpenMS/FORMAT/FeatureXMLFile.h>
#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/METADATA/PeptideIdentification.h>

#include <algorithm>

using namespace OpenMS;
using namespace std;

//-------------------------------------------------------------
// Doxygen docu
//-------------------------------------------------------------

/**
    @page TOPP_IDConflictResolver IDConflictResolver

    @brief Resolves ambiguous annotations of features with peptide identifications.

    The peptide identifications are filtered so that only one identification
    with a single hit (with the best score) is associated to each feature. (If
    two IDs have the same best score, either one of them may be selected.)

    The the filtered identifications are added to the vector of unassigned peptides
    and also reduced to a single best hit.

*/

namespace OpenMS
{

class OPENMS_DLLAPI IDConflictResolverAlgorithm
{
public:
  static void resolve(FeatureMap & features)
  {
    resolveConflict_(features);
  }

  static void resolve(ConsensusMap & features)
  {
    resolveConflict_(features);
  }

protected:
  template<class T>
  static void resolveConflict_(T & map)
  {
    // annotate as not part of the resolution
    for (PeptideIdentification & p : map.getUnassignedPeptideIdentifications())
    {
      p.setMetaValue("feature_id", "not mapped"); // not mapped to a feature
      p.setMetaValue("feature_leader", false); // and, thus, no id leader of a feature
    }

    for (auto & c : map)
    {
      c.setMetaValue("feature_id", String(c.getUniqueId()));
      resolveConflict_(c.getPeptideIdentifications(), 
        map.getUnassignedPeptideIdentifications(),
        c.getUniqueId());
    }
  }

  // compare peptide IDs by score of best hit (hits must be sorted first!)
  // (note to self: the "static" is necessary to avoid cryptic "no matching
  // function" errors from gcc when the comparator is used below)
  static bool compareIDsSmallerScores_(const PeptideIdentification & left,
                          const PeptideIdentification & right)
  {
    // if any of them is empty, the other is considered "greater"
    // independent of the score in the first hit
    if (left.getHits().empty()) return true;
    if (right.getHits().empty()) return false;
    if (left.getHits()[0].getScore() < right.getHits()[0].getScore())
    {
      return true;
    }
    return false;
  }

  static void resolveConflict_(
    vector<PeptideIdentification> & peptides, 
    vector<PeptideIdentification> & removed,
    UInt64 uid)
  {
    if (peptides.empty()) { return; }

    for (PeptideIdentification & pep : peptides)
    {
      // sort hits
      pep.sort();

      // remove all but the best hit
      if (!pep.getHits().empty())
      {
        vector<PeptideHit> best_hit(1, pep.getHits()[0]);
        pep.setHits(best_hit);
      }
      // annotate feature id
      pep.setMetaValue("feature_id", String(uid));
    }

    vector<PeptideIdentification>::iterator pos;
    if (peptides[0].isHigherScoreBetter())     // find highest-scoring ID
    {
      pos = max_element(peptides.begin(), peptides.end(), compareIDsSmallerScores_);
    }
    else  // find lowest-scoring ID
    {
      pos = min_element(peptides.begin(), peptides.end(), compareIDsSmallerScores_);
    }

    // copy conflicting ones left to best one
    for (auto it = peptides.begin(); it != pos; ++it)
    {
      it->setMetaValue("feature_leader", false);
      removed.push_back(*it);
    }
     
    // copy conflicting ones right of best one
    vector<PeptideIdentification>::iterator pos1p = pos + 1;
    for (auto it = pos1p; it != peptides.end(); ++it)
    {
      it->setMetaValue("feature_leader", false);
      removed.push_back(*it);
    }

    // set best one to first position and shrink vector
    peptides[0] = *pos;
    peptides[0].setMetaValue("feature_leader", true);
    peptides.resize(1);
  }
};

}

#endif

