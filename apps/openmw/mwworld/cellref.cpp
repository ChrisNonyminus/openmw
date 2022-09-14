#include "cellref.hpp"

#include <cassert>

#include <components/debug/debuglog.hpp>
#include <components/esm3/objectstate.hpp>

#include <components/esm4/records.hpp>

#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "esmstore.hpp"

namespace MWWorld
{

    const ESM::RefNum& CellRef::getOrAssignRefNum(ESM::RefNum& lastAssignedRefNum)
    {
        if (!mCellRef.mRefNum.isSet())
        {
            // Generated RefNums have negative mContentFile
            assert(lastAssignedRefNum.mContentFile < 0);
            lastAssignedRefNum.mIndex++;
            if (lastAssignedRefNum.mIndex == 0)  // mIndex overflow, so mContentFile should be changed
            {
                if (lastAssignedRefNum.mContentFile > std::numeric_limits<int32_t>::min())
                    lastAssignedRefNum.mContentFile--;
                else
                    Log(Debug::Error) << "RefNum counter overflow in CellRef::getOrAssignRefNum";
            }
            mCellRef.mRefNum = lastAssignedRefNum;
            mChanged = true;
        }
        return mCellRef.mRefNum;
    }

    void CellRef::unsetRefNum()
    {
        mCellRef.mRefNum.unset();
    }

    const std::string& CellRef::getRefId() const
    {
        return mCellRef.mRefID;
    }

    void CellRef::setScale(float scale)
    {
        if (scale != mCellRef.mScale)
        {
            mChanged = true;
            mCellRef.mScale = scale;
            mRefr.mScale = scale;
        }
    }

    void CellRef::setPosition(const ESM::Position &position)
    {
        mChanged = true;
        mCellRef.mPos = position;
        mRefrPos = position;
        mRefr.mPlacement.pos = { position.pos[0], position.pos[1], position.pos[2] };
        mRefr.mPlacement.rot = { position.rot[0], position.rot[1], position.rot[2] };
    }

    float CellRef::getNormalizedEnchantmentCharge(int maxCharge) const
    {
        if (maxCharge == 0)
        {
            return 0;
        }
        else if (mCellRef.mEnchantmentCharge == -1)
        {
            return 1;
        }
        else
        {
            return mCellRef.mEnchantmentCharge / static_cast<float>(maxCharge);
        }
    }

    void CellRef::setEnchantmentCharge(float charge)
    {
        if (charge != mCellRef.mEnchantmentCharge)
        {
            mChanged = true;
            mCellRef.mEnchantmentCharge = charge;
        }
    }

    void CellRef::setCharge(int charge)
    {
        if (charge != mCellRef.mChargeInt)
        {
            mChanged = true;
            mCellRef.mChargeInt = charge;
        }
    }

    void CellRef::applyChargeRemainderToBeSubtracted(float chargeRemainder)
    {
        mCellRef.mChargeIntRemainder += std::abs(chargeRemainder);
        if (mCellRef.mChargeIntRemainder > 1.0f)
        {
            float newChargeRemainder = (mCellRef.mChargeIntRemainder - std::floor(mCellRef.mChargeIntRemainder));
            if (mCellRef.mChargeInt <= static_cast<int>(mCellRef.mChargeIntRemainder))
            {
                mCellRef.mChargeInt = 0;
            }
            else
            {
                mCellRef.mChargeInt -= static_cast<int>(mCellRef.mChargeIntRemainder);
            }
            mCellRef.mChargeIntRemainder = newChargeRemainder;
        }
    }

    void CellRef::setChargeFloat(float charge)
    {
        if (charge != mCellRef.mChargeFloat)
        {
            mChanged = true;
            mCellRef.mChargeFloat = charge;
        }
    }

    void CellRef::resetGlobalVariable()
    {
        if (!mCellRef.mGlobalVariable.empty())
        {
            mChanged = true;
            mCellRef.mGlobalVariable.erase();
        }
    }

    void CellRef::setFactionRank(int factionRank)
    {
        if (factionRank != mCellRef.mFactionRank)
        {
            mChanged = true;
            mCellRef.mFactionRank = factionRank;
            mRefr.mFactionRank = factionRank;
        }
    }

    const std::string& CellRef::getOwner() const
    {
        if (mIsTes4)
            return MWBase::Environment::get().getWorld()->getStore().getFormName(mRefr.mOwner);
        else
            return mCellRef.mOwner;
    }

    void CellRef::setOwner(const std::string& owner)
    {
        if (owner != mCellRef.mOwner)
        {
            mChanged = true;
            mCellRef.mOwner = owner;
            //mRefr.mOwner = owner;
        }
    }

    void CellRef::setSoul(std::string_view soul)
    {
        if (soul != mCellRef.mSoul)
        {
            mChanged = true;
            mCellRef.mSoul = soul;
        }
    }

    const std::string& CellRef::getFaction() const
    {
        if (mIsTes4)
        {
            if (const auto* owner = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Faction>().search(mRefr.mOwner))
                return owner->mEditorId;
            return "";
        }
        return mCellRef.mFaction;
    }

    void CellRef::setFaction(const std::string& faction)
    {
        if (mIsTes4)
        {
            if (faction != getFaction())
            {
                if (const auto* owner = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Faction>().search(faction))
                {
                    mChanged = true;
                    mRefr.mOwner = owner->mFormId;
                }
            }
        }
        else if (faction != mCellRef.mFaction)
        {
            mChanged = true;
            mCellRef.mFaction = faction;
        }
    }

    void CellRef::setLockLevel(int lockLevel)
    {
        if (lockLevel != mCellRef.mLockLevel)
        {
            mChanged = true;
            mCellRef.mLockLevel = lockLevel;
            mRefr.mLockLevel = lockLevel;
        }
    }

    void CellRef::lock(int lockLevel)
    {
        if(lockLevel != 0)
            setLockLevel(abs(lockLevel)); //Changes lock to locklevel, if positive
        else
            setLockLevel(ESM::UnbreakableLock); // If zero, set to max lock level
    }

    void CellRef::unlock()
    {
        setLockLevel(-abs(mCellRef.mLockLevel)); //Makes lockLevel negative
    }

    void CellRef::setTrap(const std::string& trap)
    {
        if (trap != mCellRef.mTrap)
        {
            mChanged = true;
            mCellRef.mTrap = trap;
        }
    }

    void CellRef::setGoldValue(int value)
    {
        if (value != mCellRef.mGoldValue)
        {
            mChanged = true;
            mCellRef.mGoldValue = value;
        }
    }

    void CellRef::writeState(ESM::ObjectState &state) const
    {
        state.mRef = mCellRef;
    }

}
