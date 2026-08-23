#include "rules_mgs1.h"

#include <array>

namespace bb::codename {
namespace {

constexpr TierMask kTier[4] = {kX, kH, kN, kE | kVe};

struct Fam {
    const char* name;
    std::span<const Cond> conds;
    bool needs_time = false;
};

constexpr Cond kLadder[] = {
    {StatId::Alerts, Op::Lt, 4},       {StatId::Kills, Op::Lt, 25},
    {StatId::RationsUsed, Op::Le, 1},  {StatId::Continues, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 3},
};

constexpr Cond kScorpion[] = {{StatId::Alerts, Op::Lt, 30}, {StatId::Kills, Op::Lt, 17}};
constexpr Cond kJaguar[] = {{StatId::Alerts, Op::Lt, 30}, {StatId::Kills, Op::Gt, 62}};

constexpr Cond kEagle[] = {{StatId::PlayTimeHours, Op::Lt, 2.5}};
constexpr Cond kOrca[] = {{StatId::Kills, Op::Gt, 250}};
constexpr Cond kWhale[] = {{StatId::RationsUsed, Op::Gt, 130}};
constexpr Cond kHippo[] = {{StatId::Saves, Op::Gt, 80}};
constexpr Cond kPanda[] = {{StatId::PlayTimeHours, Op::Gt, 18}};

constexpr Cond kNightOwlA[] = {{StatId::Alerts, Op::Ge, 30}, {StatId::Alerts, Op::Le, 54},
                               {StatId::Kills, Op::Lt, 51}};
constexpr Cond kNightOwlB[] = {{StatId::Alerts, Op::Ge, 55}, {StatId::Kills, Op::Lt, 58}};
constexpr Cond kCrocA[] = {{StatId::Alerts, Op::Ge, 30},   {StatId::Alerts, Op::Le, 54},
                           {StatId::Kills, Op::Gt, 162}};
constexpr Cond kCrocB[] = {{StatId::Alerts, Op::Ge, 55}, {StatId::Kills, Op::Gt, 93}};
constexpr Cond kTazA[] = {{StatId::Alerts, Op::Lt, 30}, {StatId::Kills, Op::Gt, 18}};
constexpr Cond kTazB[] = {{StatId::Alerts, Op::Ge, 30},  {StatId::Alerts, Op::Le, 54},
                          {StatId::Kills, Op::Lt, 52}};
constexpr Cond kTazC[] = {{StatId::Alerts, Op::Ge, 55}, {StatId::Kills, Op::Gt, 59}};
constexpr Cond kWorst[] = {{StatId::Kills, Op::Gt, 130},
                           {StatId::RationsUsed, Op::Gt, 130},
                           {StatId::Saves, Op::Gt, 80},
                           {StatId::PlayTimeHours, Op::Gt, 18}};

const std::array<RankRule, 64> kMgs1Rules{{
    RankRule{"BIG BOSS", kTier[0], Kind::Elite, kLadder, true},
    RankRule{"FOX", kTier[1], Kind::Elite, kLadder, true},
    RankRule{"DOBERMAN", kTier[2], Kind::Elite, kLadder, true},
    RankRule{"HOUND", kTier[3], Kind::Elite, kLadder, true},

    RankRule{"Scorpion", kTier[0], Kind::Special, kScorpion},
    RankRule{"Centipede", kTier[1], Kind::Special, kScorpion},
    RankRule{"Tarantula", kTier[2], Kind::Special, kScorpion},
    RankRule{"Spider", kTier[3], Kind::Special, kScorpion},

    RankRule{"Jaguar", kTier[0], Kind::Special, kJaguar},
    RankRule{"Panther", kTier[1], Kind::Special, kJaguar},
    RankRule{"Leopard", kTier[2], Kind::Special, kJaguar},
    RankRule{"Puma", kTier[3], Kind::Special, kJaguar},

    RankRule{"Eagle", kTier[0], Kind::Special, kEagle, true},
    RankRule{"Hawk", kTier[1], Kind::Special, kEagle, true},
    RankRule{"Falcon", kTier[2], Kind::Special, kEagle, true},
    RankRule{"Pigeon", kTier[3], Kind::Special, kEagle, true},

    RankRule{"Orca", kTier[0], Kind::Special, kOrca},
    RankRule{"Jaws", kTier[1], Kind::Special, kOrca},
    RankRule{"Shark", kTier[2], Kind::Special, kOrca},
    RankRule{"Piranha", kTier[3], Kind::Special, kOrca},

    RankRule{"Whale", kTier[0], Kind::Special, kWhale},
    RankRule{"Mammoth", kTier[1], Kind::Special, kWhale},
    RankRule{"Elephant", kTier[2], Kind::Special, kWhale},
    RankRule{"Pig", kTier[3], Kind::Special, kWhale},

    RankRule{"Hippopotamus", kTier[0], Kind::Special, kHippo},
    RankRule{"Zebra", kTier[1], Kind::Special, kHippo},
    RankRule{"Deer", kTier[2], Kind::Special, kHippo},
    RankRule{"Cat", kTier[3], Kind::Special, kHippo},

    RankRule{"Giant Panda", kTier[0], Kind::Special, kPanda, true},
    RankRule{"Sloth", kTier[1], Kind::Special, kPanda, true},
    RankRule{"Capybara", kTier[2], Kind::Special, kPanda, true},
    RankRule{"Koala", kTier[3], Kind::Special, kPanda, true},

    RankRule{"Night Owl", kTier[0], Kind::Special, kNightOwlA},
    RankRule{"Flying Fox", kTier[1], Kind::Special, kNightOwlA},
    RankRule{"Bat", kTier[2], Kind::Special, kNightOwlA},
    RankRule{"Flying Squirrel", kTier[3], Kind::Special, kNightOwlA},

    RankRule{"Night Owl", kTier[0], Kind::Special, kNightOwlB},
    RankRule{"Flying Fox", kTier[1], Kind::Special, kNightOwlB},
    RankRule{"Bat", kTier[2], Kind::Special, kNightOwlB},
    RankRule{"Flying Squirrel", kTier[3], Kind::Special, kNightOwlB},

    RankRule{"Crocodile", kTier[0], Kind::Special, kCrocA},
    RankRule{"Alligator", kTier[1], Kind::Special, kCrocA},
    RankRule{"Iguana", kTier[2], Kind::Special, kCrocA},
    RankRule{"Comodo Dragon", kTier[3], Kind::Special, kCrocA},

    RankRule{"Crocodile", kTier[0], Kind::Special, kCrocB},
    RankRule{"Alligator", kTier[1], Kind::Special, kCrocB},
    RankRule{"Iguana", kTier[2], Kind::Special, kCrocB},
    RankRule{"Comodo Dragon", kTier[3], Kind::Special, kCrocB},

    RankRule{"Tasmanian Devil", kTier[0], Kind::Special, kTazA},
    RankRule{"Jackal", kTier[1], Kind::Special, kTazA},
    RankRule{"Hyena", kTier[2], Kind::Special, kTazA},
    RankRule{"Mongoose", kTier[3], Kind::Special, kTazA},

    RankRule{"Tasmanian Devil", kTier[0], Kind::Regular, kTazB},
    RankRule{"Jackal", kTier[1], Kind::Regular, kTazB},
    RankRule{"Hyena", kTier[2], Kind::Regular, kTazB},
    RankRule{"Mongoose", kTier[3], Kind::Regular, kTazB},

    RankRule{"Tasmanian Devil", kTier[0], Kind::Regular, kTazC},
    RankRule{"Jackal", kTier[1], Kind::Regular, kTazC},
    RankRule{"Hyena", kTier[2], Kind::Regular, kTazC},
    RankRule{"Mongoose", kTier[3], Kind::Regular, kTazC},

    RankRule{"Ostrich", kTier[0], Kind::Worst, kWorst, true},
    RankRule{"Rabbit", kTier[1], Kind::Worst, kWorst, true},
    RankRule{"Mouse", kTier[2], Kind::Worst, kWorst, true},
    RankRule{"Chicken", kTier[3], Kind::Worst, kWorst, true},
}};

static_assert(std::size(kMgs1Rules) == 64);

} // namespace

std::span<const RankRule> mgs1_rules()
{
    return kMgs1Rules;
}

}
