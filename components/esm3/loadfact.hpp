#ifndef OPENMW_ESM_FACT_H
#define OPENMW_ESM_FACT_H

#include <map>
#include <string>

#include "components/esm/defs.hpp"
#include "components/esm/refid.hpp"

namespace ESM
{

    class ESMReader;
    class ESMWriter;

    /*
     * Faction definitions
     */

    // Requirements for each rank
    struct RankData
    {
        int mAttribute1, mAttribute2; // Attribute level

        // Skill level (faction skills given in
        // skillID below.) You need one skill at
        // level 'mPrimarySkill' and two skills at level
        // 'mFavouredSkill' to advance to this rank.
        int mPrimarySkill, mFavouredSkill;

        int mFactReaction; // Reaction from faction members
    };

    struct Faction
    {
        constexpr static RecNameInts sRecordId = REC_FACT;

        /// Return a string descriptor for this record type. Currently used for debugging / error logs only.
        static std::string_view getRecordType() { return "Faction"; }

        unsigned int mRecordFlags;
        std::string mName;
        RefId mId;

        struct FADTstruct
        {
            // Which attributes we like
            int mAttribute[2];

            RankData mRankData[10];

            int mSkills[7]; // IDs of skills this faction require
                            // Each element will either contain an Skill index, or -1.

            int mIsHidden; // 1 - hidden from player

            int& getSkill(int index, bool ignored = false);
            ///< Throws an exception for invalid values of \a index.

            int getSkill(int index, bool ignored = false) const;
            ///< Throws an exception for invalid values of \a index.
        }; // 240 bytes

        FADTstruct mData;

        // <Faction ID, Reaction>
        std::map<ESM::RefId, int> mReactions;

        // Name of faction ranks (may be empty for NPC factions)
        std::string mRanks[10];

        void load(ESMReader& esm, bool& isDeleted);
        void save(ESMWriter& esm, bool isDeleted = false) const;

        void blank();
        ///< Set record to default state (does not touch the ID/index).
    };
}
#endif
