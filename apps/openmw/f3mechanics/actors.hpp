#ifndef GAME_F3MECHANICS_ACTORS_H
#define GAME_F3MECHANICS_ACTORS_H

#include <set>
#include <vector>
#include <string>
#include <list>
#include <map>

#include "usings.hpp"
#include "stats.hpp"

#include "../basemechanics/actors.hpp"

namespace ESM4
{
    class Reader;
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

namespace F3Mechanics
{
    float calcBaseSkill(const MWWorld::Ptr& ptr, F3Mechanics::Skill skill);
    class Actors : public Mechanics::Actors
    {
        public:

            Actors();

            std::list<Actor>::const_iterator begin() const { return mActors.begin(); }
            std::list<Actor>::const_iterator end() const { return mActors.end(); }
            std::size_t size() const { return mActors.size(); }

            void notifyDied(const MWWorld::Ptr &actor) override;

            /// Check if the target actor was detected by an observer
            /// If the observer is a non-NPC, check all actors in AI processing distance as observers
            bool isActorDetected(const MWWorld::Ptr& actor, const MWWorld::Ptr& observer) const override;

            /// Update magic effects for an actor. Usually done automatically once per frame, but if we're currently
            /// paused we may want to do it manually (after equipping permanent enchantment)
            void updateMagicEffects(const MWWorld::Ptr& ptr) const override;

            void updateProcessingRange() override;
            float getProcessingRange() const override;

            void addActor (const MWWorld::Ptr& ptr, bool updateImmediately=false) override;
            ///< Register an actor for stats management
            ///
            /// \note Dead actors are ignored.

            void removeActor (const MWWorld::Ptr& ptr, bool keepActive) override;
            ///< Deregister an actor for stats management
            ///
            /// \note Ignored, if \a ptr is not a registered actor.

            void resurrect(const MWWorld::Ptr& ptr) const override;

            void castSpell(const MWWorld::Ptr& ptr, const std::string& spellId, bool manualSpell = false) const override;

            void updateActor(const MWWorld::Ptr &old, const MWWorld::Ptr& ptr) const override;
            ///< Updates an actor with a new Ptr

            void dropActors (const MWWorld::CellStore *cellStore, const MWWorld::Ptr& ignore) override;
            ///< Deregister all actors (except for \a ignore) in the given cell.

            void updateCombatMusic() override;
            ///< Update combat music state

            void update (float duration, bool paused) override;
            ///< Update actor stats and store desired velocity vectors in \a movement

            void updateActor(const MWWorld::Ptr& ptr, float duration) const override;
            ///< This function is normally called automatically during the update process, but it can
            /// also be called explicitly at any time to force an update.

            /// Removes an actor from combat and makes all of their allies stop fighting the actor's targets
            void stopCombat(const MWWorld::Ptr& ptr) const override;

            void playIdleDialogue(const MWWorld::Ptr& actor) const override;
            void updateMovementSpeed(const MWWorld::Ptr& actor) const override;
            void updateGreetingState(const MWWorld::Ptr& actor, Actor& actorState, bool turnOnly) override;
            void turnActorToFacePlayer(const MWWorld::Ptr& actor, Actor& actorState, const osg::Vec3f& dir) const override;

            void rest(double hours, bool sleep) const override;
            ///< Update actors while the player is waiting or sleeping.

            void updateSneaking(CharacterController* ctrl, float duration) override;
            ///< Update the sneaking indicator state according to the given player character controller.

            void restoreDynamicStats(const MWWorld::Ptr& actor, double hours, bool sleep) const override;

            int getHoursToRest(const MWWorld::Ptr& ptr) const override;
            ///< Calculate how many hours the given actor needs to rest in order to be fully healed

            void fastForwardAi() const override;
            ///< Simulate the passing of time

            int countDeaths (const std::string& id) const override;
            ///< Return the number of deaths for actors with the given ID.

            bool isAttackPreparing(const MWWorld::Ptr& ptr) const override;
            bool isRunning(const MWWorld::Ptr& ptr) const override;
            bool isSneaking(const MWWorld::Ptr& ptr) const override;

            void forceStateUpdate(const MWWorld::Ptr &ptr) const override;

            bool playAnimationGroup(const MWWorld::Ptr& ptr, std::string_view groupName, int mode,
                int number, bool persist = false) const override;
            void skipAnimation(const MWWorld::Ptr& ptr) const override;
            bool checkAnimationPlaying(const MWWorld::Ptr& ptr, const std::string& groupName) const override;
            void persistAnimationStates() const override;

            void getObjectsInRange(const osg::Vec3f& position, float radius, std::vector<MWWorld::Ptr>& out) const override;

            bool isAnyObjectInRange(const osg::Vec3f& position, float radius) const override;

            ///Returns the list of actors which are siding with the given actor in fights
            /**ie AiFollow or AiEscort is active and the target is the actor **/
            std::vector<MWWorld::Ptr> getActorsSidingWith(const MWWorld::Ptr& actor,
                bool excludeInfighting = false) const override;
            std::vector<MWWorld::Ptr> getActorsFollowing(const MWWorld::Ptr& actor) const override;

            /// Recursive version of getActorsFollowing
            void getActorsFollowing(const MWWorld::Ptr &actor, std::set<MWWorld::Ptr>& out) const override;
            /// Recursive version of getActorsSidingWith
            void getActorsSidingWith(const MWWorld::Ptr &actor, std::set<MWWorld::Ptr>& out,
                bool excludeInfighting = false) const override;

            /// Get the list of AiFollow::mFollowIndex for all actors following this target
            std::vector<int> getActorsFollowingIndices(const MWWorld::Ptr& actor) const override;
            std::map<int, MWWorld::Ptr> getActorsFollowingByIndex(const MWWorld::Ptr& actor) const override;

            ///Returns the list of actors which are fighting the given actor
            /**ie AiCombat is active and the target is the actor **/
            std::vector<MWWorld::Ptr> getActorsFighting(const MWWorld::Ptr& actor) const override;

            /// Unlike getActorsFighting, also returns actors that *would* fight the given actor if they saw him.
            std::vector<MWWorld::Ptr> getEnemiesNearby(const MWWorld::Ptr& actor) const override;

            void write (ESM::ESMWriter& writer, Loading::Listener& listener) const override {}

            void readRecord (ESM::ESMReader& reader, uint32_t type) override {}

            void clear(); // Clear death counter

            bool isCastingSpell(const MWWorld::Ptr& ptr) const override;
            bool isReadyToBlock(const MWWorld::Ptr& ptr) const override;
            bool isAttackingOrSpell(const MWWorld::Ptr& ptr) const override;

            int getGreetingTimer(const MWWorld::Ptr& ptr) const override;
            float getAngleToPlayer(const MWWorld::Ptr& ptr) const override;
            Mechanics::GreetingState getGreetingState(const MWWorld::Ptr& ptr) const override;
            bool isTurningToPlayer(const MWWorld::Ptr& ptr) const override;

        private:
        
            void updateVisibility(const MWWorld::Ptr& ptr, CharacterController& ctrl) const override;

            void adjustMagicEffects(const MWWorld::Ptr& creature, float duration) const override;

            void calculateRestoration(const MWWorld::Ptr& ptr, float duration) const override;

            void updateCrimePursuit(const MWWorld::Ptr& ptr, float duration) const override;

            void killDeadActors () override;

            void purgeSpellEffects(int casterActorId) const override;

            void predictAndAvoidCollisions(float duration) const override;

            /** Start combat between two actors
                @Notes: If againstPlayer = true then actor2 should be the Player.
                        If one of the combatants is creature it should be actor1.
            */
            void engageCombat(const MWWorld::Ptr& actor1, const MWWorld::Ptr& actor2,
                std::map<const MWWorld::Ptr, const std::set<MWWorld::Ptr>>& cachedAllies, bool againstPlayer) const override;

            /// Recursive version of getActorsSidingWith that takes, adds to and returns a cache of
            /// actors mapped to their allies. Excludes infighting
            void getActorsSidingWith(const MWWorld::Ptr &actor, std::set<MWWorld::Ptr>& out,
                std::map<const MWWorld::Ptr, const std::set<MWWorld::Ptr>>& cachedAllies) const override;
    };
}

#endif
