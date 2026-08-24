#include "rules_mgs1.h"

#include <array>

namespace bb::codename {
namespace {

constexpr TierMask kTier[4] = {kX, kH, kN, kE | kVe};

constexpr Cond kLadder[] = {
    {StatId::Alerts, Op::Lt, 4},      {StatId::Kills, Op::Lt, 25},
    {StatId::RationsUsed, Op::Le, 1}, {StatId::Continues, Op::Eq, 0},
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
constexpr Cond kCrocA[] = {{StatId::Alerts, Op::Ge, 30}, {StatId::Alerts, Op::Le, 54},
                           {StatId::Kills, Op::Gt, 162}};
constexpr Cond kCrocB[] = {{StatId::Alerts, Op::Ge, 55}, {StatId::Kills, Op::Gt, 93}};
constexpr Cond kTazA[] = {{StatId::Alerts, Op::Lt, 30}, {StatId::Kills, Op::Gt, 18}};
constexpr Cond kTazB[] = {{StatId::Alerts, Op::Ge, 30}, {StatId::Alerts, Op::Le, 54},
                          {StatId::Kills, Op::Lt, 52}};
constexpr Cond kTazC[] = {{StatId::Alerts, Op::Ge, 55}, {StatId::Kills, Op::Gt, 59}};
constexpr Cond kWorst[] = {{StatId::Kills, Op::Gt, 130},
                           {StatId::RationsUsed, Op::Gt, 130},
                           {StatId::Saves, Op::Gt, 80},
                           {StatId::PlayTimeHours, Op::Gt, 18}};

const std::array<RankRule, 64> kRules{{
    {"BIG BOSS", kTier[0], Kind::Elite, kLadder, true},
    {"FOX", kTier[1], Kind::Elite, kLadder, true},
    {"DOBERMAN", kTier[2], Kind::Elite, kLadder, true},
    {"HOUND", kTier[3], Kind::Elite, kLadder, true},

    {"Scorpion", kTier[0], Kind::Special, kScorpion},
    {"Centipede", kTier[1], Kind::Special, kScorpion},
    {"Tarantula", kTier[2], Kind::Special, kScorpion},
    {"Spider", kTier[3], Kind::Special, kScorpion},

    {"Jaguar", kTier[0], Kind::Special, kJaguar},
    {"Panther", kTier[1], Kind::Special, kJaguar},
    {"Leopard", kTier[2], Kind::Special, kJaguar},
    {"Puma", kTier[3], Kind::Special, kJaguar},

    {"Eagle", kTier[0], Kind::Special, kEagle, true},
    {"Hawk", kTier[1], Kind::Special, kEagle, true},
    {"Falcon", kTier[2], Kind::Special, kEagle, true},
    {"Pigeon", kTier[3], Kind::Special, kEagle, true},

    {"Orca", kTier[0], Kind::Special, kOrca},
    {"Jaws", kTier[1], Kind::Special, kOrca},
    {"Shark", kTier[2], Kind::Special, kOrca},
    {"Piranha", kTier[3], Kind::Special, kOrca},

    {"Whale", kTier[0], Kind::Special, kWhale},
    {"Mammoth", kTier[1], Kind::Special, kWhale},
    {"Elephant", kTier[2], Kind::Special, kWhale},
    {"Pig", kTier[3], Kind::Special, kWhale},

    {"Hippopotamus", kTier[0], Kind::Special, kHippo},
    {"Zebra", kTier[1], Kind::Special, kHippo},
    {"Deer", kTier[2], Kind::Special, kHippo},
    {"Cat", kTier[3], Kind::Special, kHippo},

    {"Giant Panda", kTier[0], Kind::Special, kPanda, true},
    {"Sloth", kTier[1], Kind::Special, kPanda, true},
    {"Capybara", kTier[2], Kind::Special, kPanda, true},
    {"Koala", kTier[3], Kind::Special, kPanda, true},

    {"Night Owl", kTier[0], Kind::Special, kNightOwlA},
    {"Flying Fox", kTier[1], Kind::Special, kNightOwlA},
    {"Bat", kTier[2], Kind::Special, kNightOwlA},
    {"Flying Squirrel", kTier[3], Kind::Special, kNightOwlA},
    {"Night Owl", kTier[0], Kind::Special, kNightOwlB},
    {"Flying Fox", kTier[1], Kind::Special, kNightOwlB},
    {"Bat", kTier[2], Kind::Special, kNightOwlB},
    {"Flying Squirrel", kTier[3], Kind::Special, kNightOwlB},

    {"Crocodile", kTier[0], Kind::Special, kCrocA},
    {"Alligator", kTier[1], Kind::Special, kCrocA},
    {"Iguana", kTier[2], Kind::Special, kCrocA},
    {"Komodo Dragon", kTier[3], Kind::Special, kCrocA},
    {"Crocodile", kTier[0], Kind::Special, kCrocB},
    {"Alligator", kTier[1], Kind::Special, kCrocB},
    {"Iguana", kTier[2], Kind::Special, kCrocB},
    {"Komodo Dragon", kTier[3], Kind::Special, kCrocB},

    {"Tasmanian Devil", kTier[0], Kind::Special, kTazA},
    {"Jackal", kTier[1], Kind::Special, kTazA},
    {"Hyena", kTier[2], Kind::Special, kTazA},
    {"Mongoose", kTier[3], Kind::Special, kTazA},
    {"Tasmanian Devil", kTier[0], Kind::Regular, kTazB},
    {"Jackal", kTier[1], Kind::Regular, kTazB},
    {"Hyena", kTier[2], Kind::Regular, kTazB},
    {"Mongoose", kTier[3], Kind::Regular, kTazB},
    {"Tasmanian Devil", kTier[0], Kind::Regular, kTazC},
    {"Jackal", kTier[1], Kind::Regular, kTazC},
    {"Hyena", kTier[2], Kind::Regular, kTazC},
    {"Mongoose", kTier[3], Kind::Regular, kTazC},

    {"Ostrich", kTier[0], Kind::Worst, kWorst, true},
    {"Rabbit", kTier[1], Kind::Worst, kWorst, true},
    {"Mouse", kTier[2], Kind::Worst, kWorst, true},
    {"Chicken", kTier[3], Kind::Worst, kWorst, true},
}};

} // namespace

std::span<const RankRule> mgs1_integral_rules()
{
    return kRules;
}

} // namespace bb::codename
