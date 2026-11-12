#include <orchard/ai/world.hpp>

#include <map>

namespace orchard {

const std::vector<std::string>& Agent::brains() {
    static const std::vector<std::string> all = {"zombie", "runner", "crawler", "screamer", "guard", "soldier", "stealth-guard", "brawler", "hunter", "prey", "predator", "companion", "civilian", "flyer", "turret", "custom"};
    return all;
}

const char* Agent::brain_label(const std::string& b) {
    if (b == "zombie") return "Zombie (shambles, follows noise, bites)";
    if (b == "runner") return "Runner zombie (sprints at you)";
    if (b == "crawler") return "Crawler zombie (low and slow)";
    if (b == "screamer") return "Screamer zombie (calls the horde)";
    if (b == "guard") return "Guard (patrols, shoots, takes cover)";
    if (b == "soldier") return "Tactical soldier (squads, cover, flanking, grenades)";
    if (b == "stealth-guard") return "Stealth guard (suspicion meter, alarms, searches)";
    if (b == "brawler") return "Brawler (melee fighter)";
    if (b == "hunter") return "Hunter (stalks you, learns your tricks)";
    if (b == "prey") return "Prey animal (grazes, herds, runs away)";
    if (b == "predator") return "Predator (stalks and pounces on animals)";
    if (b == "companion") return "Companion (follows and fights for you)";
    if (b == "civilian") return "Civilian (wanders, panics at gunfire)";
    if (b == "flyer") return "Flyer (drone, bird, bat)";
    if (b == "turret") return "Turret (stands still and shoots)";
    if (b == "custom") return "Custom (your own behavior file)";
    return "Agent";
}

std::optional<Agent> Agent::preset(const std::string& b) {
    Agent a;
    a.brain = b;
    if (b == "zombie" || b == "runner" || b == "crawler" || b == "screamer") {
        a.team = "zombies";
        a.walk_speed = b == "crawler" ? 0.6f : 1.1f;
        a.run_speed = b == "runner" ? 6.0f : b == "crawler" ? 0.9f : 1.6f;
        a.sight_range = 18.0f;
        a.sight_angle = 140.0f;
        a.hearing = 1.6f;
        a.awareness = 2.0f;
        a.reaction = 0.6f;
        a.attack_range = 1.3f;
        a.attack_damage = b == "runner" ? 8.0f : 15.0f;
        a.attack_knockback = b == "runner" ? 25.0f : 40.0f;
        a.attack_rate = 0.8f;
        a.flee_health = 0.0f;
        a.aggression = 1.0f;
        a.shoot_effect.clear();
        if (b == "crawler") {
            a.height = 0.7f;
            a.eye_height = 0.4f;
        }
        return a;
    }
    if (b == "guard" || b == "soldier" || b == "stealth-guard") {
        a.team = "enemies";
        a.ranged = true;
        a.damage_type = "projectile";
        a.attack_range = 22.0f;
        a.attack_damage = 12.0f;
        a.attack_knockback = 3.0f;
        a.attack_rate = b == "soldier" ? 5.0f : 2.5f;
        a.accuracy = b == "soldier" ? 0.7f : 0.55f;
        a.magazine = b == "soldier" ? 30 : 12;
        a.awareness = b == "stealth-guard" ? 0.6f : 1.5f;
        a.sight_angle = b == "stealth-guard" ? 100.0f : 120.0f;
        a.flee_health = b == "soldier" ? 0.25f : 0.15f;
        if (b == "soldier") a.squad = "squad";
        return a;
    }
    if (b == "brawler") {
        a.team = "enemies";
        a.attack_range = 1.6f;
        a.attack_damage = 18.0f;
        a.attack_knockback = 80.0f;
        a.attack_rate = 1.2f;
        a.run_speed = 5.5f;
        a.shoot_effect.clear();
        return a;
    }
    if (b == "hunter") {
        a.team = "hunter";
        a.walk_speed = 1.8f;
        a.run_speed = 7.0f;
        a.sight_range = 30.0f;
        a.sight_angle = 150.0f;
        a.hearing = 2.2f;
        a.awareness = 1.4f;
        a.reaction = 0.2f;
        a.attack_range = 1.8f;
        a.attack_damage = 1000.0f;
        a.attack_knockback = 250.0f;
        a.attack_rate = 0.5f;
        a.flee_health = 0.0f;
        a.height = 2.4f;
        a.radius = 0.5f;
        a.eye_height = 2.1f;
        a.shoot_effect.clear();
        return a;
    }
    if (b == "prey") {
        a.team = "animals";
        a.walk_speed = 1.2f;
        a.run_speed = 7.0f;
        a.sight_range = 30.0f;
        a.sight_angle = 300.0f;
        a.hearing = 2.0f;
        a.attack_damage = 0.0f;
        a.attack_knockback = 0.0f;
        a.height = 1.2f;
        a.eye_height = 1.1f;
        a.flee_health = 1.0f;
        a.shoot_effect.clear();
        return a;
    }
    if (b == "predator") {
        a.team = "predators";
        a.targets = "animals,player";
        a.walk_speed = 1.5f;
        a.run_speed = 8.0f;
        a.sight_range = 35.0f;
        a.attack_range = 1.5f;
        a.attack_damage = 30.0f;
        a.attack_knockback = 120.0f;
        a.hit_effect = "bleeding";
        a.hit_effect_time = 6.0f;
        a.hit_effect_chance = 0.4f;
        a.height = 1.1f;
        a.eye_height = 0.9f;
        a.flee_health = 0.1f;
        a.shoot_effect.clear();
        return a;
    }
    if (b == "companion") {
        a.team = "player";
        a.ranged = true;
        a.damage_type = "projectile";
        a.attack_range = 20.0f;
        a.attack_damage = 10.0f;
        a.attack_knockback = 3.0f;
        a.attack_rate = 2.0f;
        a.accuracy = 0.65f;
        a.run_speed = 5.5f;
        a.flee_health = 0.0f;
        return a;
    }
    if (b == "civilian") {
        a.team = "civilians";
        a.walk_speed = 1.3f;
        a.run_speed = 5.0f;
        a.attack_damage = 0.0f;
        a.attack_knockback = 0.0f;
        a.flee_health = 1.0f;
        a.shoot_effect.clear();
        return a;
    }
    if (b == "flyer") {
        a.team = "enemies";
        a.flying = true;
        a.walk_speed = 3.0f;
        a.run_speed = 9.0f;
        a.ranged = true;
        a.damage_type = "projectile";
        a.attack_range = 15.0f;
        a.attack_damage = 6.0f;
        a.attack_knockback = 2.0f;
        a.attack_rate = 3.0f;
        a.accuracy = 0.5f;
        a.sight_angle = 220.0f;
        a.height = 0.6f;
        a.eye_height = 0.0f;
        a.radius = 0.4f;
        return a;
    }
    if (b == "turret") {
        a.team = "enemies";
        a.walk_speed = 0.0f;
        a.run_speed = 0.0f;
        a.ranged = true;
        a.damage_type = "projectile";
        a.attack_range = 30.0f;
        a.attack_damage = 8.0f;
        a.attack_knockback = 3.0f;
        a.attack_rate = 6.0f;
        a.accuracy = 0.75f;
        a.magazine = 0;
        a.turn_speed = 160.0f;
        a.sight_angle = 200.0f;
        a.eye_height = 0.5f;
        a.flee_health = 0.0f;
        return a;
    }
    if (b == "custom") return a;
    return std::nullopt;
}

namespace ai {

namespace {

const std::map<std::string, std::string>& behaviors() {
    static const std::map<std::string, std::string> all = {
        {"zombie", R"BT(selector
  sequence
    ? target_in_reach
    face_target
    attack
  sequence
    ? has_target
    chase
  sequence
    ? heard_noise
    go_to_noise
    look_around 2
  sequence
    ? suspicious
    face_noise
    go_to_noise
  random
    wander 8 weight=3
    wait 2 6 weight=2
)BT"},
        {"runner", R"BT(selector
  sequence
    ? target_in_reach
    face_target
    attack
  sequence
    ? has_target
    chase run
  sequence
    ? heard_noise
    go_to_noise run
    look_around 1.5
  sequence
    ? suspicious
    face_noise
    go_to_noise
  random
    wander 10 weight=3
    wait 1 4 weight=1
)BT"},
        {"crawler", R"BT(selector
  sequence
    ? target_in_reach
    attack
  sequence
    ? has_target
    chase
  sequence
    ? heard_noise
    go_to_noise
  sequence
    ? suspicious
    face_noise
  wait 3 8
)BT"},
        {"screamer", R"BT(selector
  sequence
    ? just_spotted
    face_target
    scream 45
  sequence
    ? target_in_reach
    face_target
    attack
  sequence
    ? has_target
    chase
  sequence
    ? heard_noise
    go_to_noise
    look_around 2
  sequence
    ? suspicious
    face_noise
    go_to_noise
  random
    wander 8 weight=3
    wait 2 6 weight=2
)BT"},
        {"guard", R"BT(selector
  sequence
    ? in_danger
    escape_danger
  sequence
    ? has_target
    ? health_below 0.15
    flee 20
  sequence
    ? can_see_target
    face_target
    selector
      sequence
        ? ammo_empty
        cooldown 6
          take_cover
        reload
      sequence
        ? target_in_range
        attack
      chase run
  sequence
    ? has_target
    chase run
  sequence
    ? suspicious
    investigate 5
  sequence
    ? searching
    search 12 20
  patrol
)BT"},
        {"soldier", R"BT(selector
  sequence
    ? in_danger
    escape_danger
  sequence
    ? has_target
    plan
  sequence
    ? heard_noise
    call_help
    investigate 4
  sequence
    ? suspicious
    investigate 5
  sequence
    ? searching
    search 15 25
  patrol
)BT"},
        {"stealth-guard", R"BT(selector
  sequence
    ? in_danger
    escape_danger
  sequence
    ? can_see_target
    ? alerted
    raise_alarm
    face_target
    selector
      sequence
        ? ammo_empty
        reload
      sequence
        ? target_in_range
        attack
      chase run
  sequence
    ? has_target
    chase run
  sequence
    ? saw_body
    raise_alarm
    investigate 8
  sequence
    ? suspicious
    investigate 6
  sequence
    ? team_alert evasion
    search 20 30
  sequence
    ? team_alert alert
    search 20 30
  sequence
    ? team_alert caution
    patrol run
  patrol
)BT"},
        {"brawler", R"BT(selector
  sequence
    ? has_target
    ? health_below 0.15
    flee 15
  sequence
    ? target_in_reach
    face_target
    attack
  sequence
    ? has_target
    chase run
  sequence
    ? suspicious
    investigate 4
  sequence
    ? searching
    search 10 15
  patrol
)BT"},
        {"hunter", R"BT(selector
  sequence
    ? fear_fire
    flee 25
  sequence
    ? target_in_reach
    face_target
    attack
  sequence
    ? can_see_target
    chase run
  sequence
    ? has_target
    chase run
  sequence
    ? heard_noise
    go_to_noise run
    look_around 2
  sequence
    ? director_backoff
    backstage
  sequence
    ? searching
    sequence
      ? director_unlocked hiding 2
      check_hiding_spots
  sequence
    ? searching
    search 10 15
  hunt_hint
)BT"},
        {"prey", R"BT(selector
  sequence
    ? threatened 14
    flee 30
  sequence
    ? panicked
    flee 25
  random
    graze 3 8 weight=3
    wander 10 weight=2
    flock weight=2
)BT"},
        {"predator", R"BT(selector
  sequence
    ? has_target
    ? health_below 0.1
    flee 30
  sequence
    ? target_in_reach
    face_target
    attack
  sequence
    ? can_see_target
    selector
      sequence
        ? target_closer 9
        chase run
      stalk
  sequence
    ? has_target
    chase
  random
    wander 15 weight=2
    wait 2 6 weight=1
)BT"},
        {"companion", R"BT(selector
  sequence
    ? in_danger
    escape_danger
  sequence
    ? leader_far 18
    follow_leader 3 run
  sequence
    ? has_target
    ? leader_near 20
    selector
      sequence
        ? ammo_empty
        reload
      sequence
        ? can_see_target
        ? target_in_range
        face_target
        attack
      chase run
  sequence
    ? leader_far 5
    follow_leader 3
  look_around 3
)BT"},
        {"civilian", R"BT(selector
  sequence
    ? panicked
    flee 30
  sequence
    ? threatened 6
    flee 15
  random
    wander 20 weight=3
    wait 3 8 weight=2
)BT"},
        {"flyer", R"BT(selector
  sequence
    ? can_see_target
    fly_attack
  sequence
    ? has_target
    fly_to_target
  fly_wander
)BT"},
        {"turret", R"BT(selector
  sequence
    ? can_see_target
    face_target
    selector
      sequence
        ? ammo_empty
        reload
      attack
  look_around 3
)BT"},
        {"custom", R"BT(selector
  sequence
    ? target_in_reach
    attack
  sequence
    ? has_target
    chase
  wait 1 3
)BT"},
    };
    return all;
}

} // namespace

const std::string& brain_behavior(const std::string& brain) {
    static const std::string none;
    const auto it = behaviors().find(brain);
    return it == behaviors().end() ? none : it->second;
}

} // namespace ai
} // namespace orchard
