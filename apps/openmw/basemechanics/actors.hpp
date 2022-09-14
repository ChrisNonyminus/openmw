#ifndef GAME_BASE_MECHANICS_ACTORS_H
#define GAME_BASE_MECHANICS_ACTORS_H

#include <set>
#include <vector>
#include <string>
#include <list>
#include <map>

#include "../mwmechanics/actor.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;
}

namespace osg
{
    class Vec3f;
}

namespace Loading
{
    class Listener;
}

namespace MWWorld
{
    class Ptr;
    class CellStore;
}

namespace Mechanics
{
    using Actor = MWMechanics::Actor; // TODO: abstract the actor class too
    using CharacterController = MWMechanics::CharacterController; // TODO: abstract the character controller class too
    using GreetingState = MWMechanics::GreetingState;

    class Actors
    {
        public:

            std::list<Actor>::const_iterator begin() const { return mActors.begin(); }
            std::list<Actor>::const_iterator end() const { return mActors.end(); }
            std::size_t size() const { return mActors.size(); }

            virtual void notifyDied(const MWWorld::Ptr &actor) = 0;

            /// Check if the target actor was detected by an observer
            /// If the observer is a non-NPC, check all actors in AI processing distance as observers
            virtual bool isActorDetected(const MWWorld::Ptr& actor, const MWWorld::Ptr& observer) const = 0;

            /// Update magic effects for an actor. Usually done automatically once per frame, but if we're currently
            /// paused we may want to do it manually (after equipping permanent enchantment)
            virtual void updateMagicEffects(const MWWorld::Ptr& ptr) const = 0;

            virtual void updateProcessingRange() = 0;
            virtual float getProcessingRange() const = 0;

            virtual void addActor (const MWWorld::Ptr& ptr, bool updateImmediately=false) = 0;
            ///< Register an actor for stats management
            ///
            /// \note Dead actors are ignored.

            virtual void removeActor (const MWWorld::Ptr& ptr, bool keepActive) = 0;
            ///< Deregister an actor for stats management
            ///
            /// \note Ignored, if \a ptr is not a registered actor.

            virtual void resurrect(const MWWorld::Ptr& ptr) const = 0;

            virtual void castSpell(const MWWorld::Ptr& ptr, const std::string& spellId, bool manualSpell = false) const = 0;

            virtual void updateActor(const MWWorld::Ptr &old, const MWWorld::Ptr& ptr) const = 0;
            ///< Updates an actor with a new Ptr

            virtual void dropActors (const MWWorld::CellStore *cellStore, const MWWorld::Ptr& ignore) = 0;
            ///< Deregister all actors (except for \a ignore) in the given cell.

            virtual void updateCombatMusic() = 0;
            ///< Update combat music state

            virtual void update (float duration, bool paused) = 0;
            ///< Update actor stats and store desired velocity vectors in \a movement

            virtual void updateActor(const MWWorld::Ptr& ptr, float duration) const = 0;
            ///< This function is normally called automatically during the update process, but it can
            /// also be called explicitly at any time to force an update.

            /// Removes an actor from combat and makes all of their allies stop fighting the actor's targets
            virtual void stopCombat(const MWWorld::Ptr& ptr) const = 0;

            virtual void playIdleDialogue(const MWWorld::Ptr& actor) const = 0;
            virtual void updateMovementSpeed(const MWWorld::Ptr& actor) const = 0;
            virtual void updateGreetingState(const MWWorld::Ptr& actor, Actor& actorState, bool turnOnly) = 0;
            virtual void turnActorToFacePlayer(const MWWorld::Ptr& actor, Actor& actorState, const osg::Vec3f& dir) const = 0;

            virtual void rest(double hours, bool sleep) const = 0;
            ///< Update actors while the player is waiting or sleeping.

            virtual void updateSneaking(CharacterController* ctrl, float duration) = 0;
            ///< Update the sneaking indicator state according to the given player character controller.

            virtual void restoreDynamicStats(const MWWorld::Ptr& actor, double hours, bool sleep) const = 0;

            virtual int getHoursToRest(const MWWorld::Ptr& ptr) const = 0;
            ///< Calculate how many hours the given actor needs to rest in order to be fully healed

            virtual void fastForwardAi() const = 0;
            ///< Simulate the passing of time

            virtual int countDeaths (const std::string& id) const = 0;
            ///< Return the number of deaths for actors with the given ID.

            virtual bool isAttackPreparing(const MWWorld::Ptr& ptr) const = 0;
            virtual bool isRunning(const MWWorld::Ptr& ptr) const = 0;
            virtual bool isSneaking(const MWWorld::Ptr& ptr) const = 0;

            virtual void forceStateUpdate(const MWWorld::Ptr &ptr) const = 0;

            virtual bool playAnimationGroup(const MWWorld::Ptr& ptr, std::string_view groupName, int mode,
                int number, bool persist = false) const = 0;
            virtual void skipAnimation(const MWWorld::Ptr& ptr) const = 0;
            virtual bool checkAnimationPlaying(const MWWorld::Ptr& ptr, const std::string& groupName) const = 0;
            virtual void persistAnimationStates() const = 0;

            virtual void getObjectsInRange(const osg::Vec3f& position, float radius, std::vector<MWWorld::Ptr>& out) const = 0;

            virtual bool isAnyObjectInRange(const osg::Vec3f& position, float radius) const = 0;

            ///Returns the list of actors which are siding with the given actor in fights
            /**ie AiFollow or AiEscort is active and the target is the actor **/
            virtual std::vector<MWWorld::Ptr> getActorsSidingWith(const MWWorld::Ptr& actor,
                bool excludeInfighting = false) const = 0;
            virtual std::vector<MWWorld::Ptr> getActorsFollowing(const MWWorld::Ptr& actor) const = 0;

            /// Recursive version of getActorsFollowing
            virtual void getActorsFollowing(const MWWorld::Ptr &actor, std::set<MWWorld::Ptr>& out) const = 0;
            /// Recursive version of getActorsSidingWith
            virtual void getActorsSidingWith(const MWWorld::Ptr &actor, std::set<MWWorld::Ptr>& out,
                bool excludeInfighting = false) const = 0;

            /// Get the list of AiFollow::mFollowIndex for all actors following this target
            virtual std::vector<int> getActorsFollowingIndices(const MWWorld::Ptr& actor) const = 0;
            virtual std::map<int, MWWorld::Ptr> getActorsFollowingByIndex(const MWWorld::Ptr& actor) const = 0;

            ///Returns the list of actors which are fighting the given actor
            /**ie AiCombat is active and the target is the actor **/
            virtual std::vector<MWWorld::Ptr> getActorsFighting(const MWWorld::Ptr& actor) const = 0;

            /// Unlike getActorsFighting, also returns actors that *would* fight the given actor if they saw him.
            virtual std::vector<MWWorld::Ptr> getEnemiesNearby(const MWWorld::Ptr& actor) const = 0;

            virtual void write (ESM::ESMWriter& writer, Loading::Listener& listener) const = 0;

            virtual void readRecord (ESM::ESMReader& reader, uint32_t type) = 0;

            virtual void clear() = 0; // Clear death counter

            virtual bool isCastingSpell(const MWWorld::Ptr& ptr) const = 0;
            virtual bool isReadyToBlock(const MWWorld::Ptr& ptr) const = 0;
            virtual bool isAttackingOrSpell(const MWWorld::Ptr& ptr) const = 0;

            virtual int getGreetingTimer(const MWWorld::Ptr& ptr) const = 0;
            virtual float getAngleToPlayer(const MWWorld::Ptr& ptr) const = 0;
            virtual GreetingState getGreetingState(const MWWorld::Ptr& ptr) const = 0;
            virtual bool isTurningToPlayer(const MWWorld::Ptr& ptr) const = 0;

        protected:
            enum class MusicType
            {
                Title,
                Explore,
                Battle,

                // fo3+
                Dungeon,
                Base,
                Public
            };

            std::map<std::string, int> mDeathCount;
            std::list<Actor> mActors;
            std::map<const MWWorld::LiveCellRefBase*, std::list<Actor>::iterator> mIndex;
            float mTimerDisposeSummonsCorpses;
            float mTimerUpdateHeadTrack = 0;
            float mTimerUpdateEquippedLight = 0;
            float mTimerUpdateHello = 0;
            float mSneakTimer = 0; // Times update of sneak icon
            float mSneakSkillTimer = 0; // Times sneak skill progress from "avoid notice"
            float mActorsProcessingRange;
            bool mSmoothMovement;
            MusicType mCurrentMusic = MusicType::Title;

            virtual void updateVisibility(const MWWorld::Ptr& ptr, CharacterController& ctrl) const = 0;

            virtual void adjustMagicEffects(const MWWorld::Ptr& creature, float duration) const = 0;

            virtual void calculateRestoration(const MWWorld::Ptr& ptr, float duration) const = 0;

            virtual void updateCrimePursuit(const MWWorld::Ptr& ptr, float duration) const = 0;

            virtual void killDeadActors () = 0;

            virtual void purgeSpellEffects(int casterActorId) const = 0;

            virtual void predictAndAvoidCollisions(float duration) const = 0;

            /** Start combat between two actors
                @Notes: If againstPlayer = true then actor2 should be the Player.
                        If one of the combatants is creature it should be actor1.
            */
            virtual void engageCombat(const MWWorld::Ptr& actor1, const MWWorld::Ptr& actor2,
                std::map<const MWWorld::Ptr, const std::set<MWWorld::Ptr>>& cachedAllies, bool againstPlayer) const = 0;

            /// Recursive version of getActorsSidingWith that takes, adds to and returns a cache of
            /// actors mapped to their allies. Excludes infighting
            virtual void getActorsSidingWith(const MWWorld::Ptr &actor, std::set<MWWorld::Ptr>& out,
                std::map<const MWWorld::Ptr, const std::set<MWWorld::Ptr>>& cachedAllies) const = 0;

    };
}

#endif
