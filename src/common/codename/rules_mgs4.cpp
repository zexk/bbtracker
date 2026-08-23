#include "rules_mgs4.h"

#include <array>
#include <set>

namespace bb::codename {
namespace {

constexpr TierMask kAnyDiff = kVe | kE | kN | kH | kX;

constexpr Cond kNoRecovery = {StatId::RationsUsed, Op::Eq, 0};
constexpr Cond kNoKills = {StatId::Kills, Op::Eq, 0};
constexpr Cond kNoCont = {StatId::Continues, Op::Eq, 0};
constexpr Cond kNoSpecial = {StatId::SpecialItemUsed, Op::Eq, 0};
constexpr Cond kNoAlerts = {StatId::Alerts, Op::Eq, 0};

constexpr Cond kBB[] = {
    kNoAlerts, kNoKills, kNoCont, kNoRecovery, kNoSpecial,
    {StatId::PlayTimeHours, Op::Lt, 5},
};

constexpr Cond kFoxHound[] = {
    {StatId::Alerts, Op::Lt, 3},      kNoKills,
    kNoCont,                          kNoRecovery,
    kNoSpecial,                       {StatId::PlayTimeHours, Op::Lt, 5.5},
};

constexpr Cond kFox[] = {
    {StatId::Alerts, Op::Lt, 5},      kNoKills,
    kNoCont,                          kNoRecovery,
    kNoSpecial,                       {StatId::PlayTimeHours, Op::Lt, 6},
};

constexpr Cond kHound[] = {
    {StatId::Alerts, Op::Lt, 3},      kNoKills,
    kNoCont,                          kNoRecovery,
    kNoSpecial,                       {StatId::PlayTimeHours, Op::Lt, 6.5},
};

constexpr Cond kMantis[] = {
    kNoAlerts, kNoKills, kNoCont, kNoRecovery,
    {StatId::PlayTimeHours, Op::Lt, 5},
};

constexpr Cond kOctopus[] = {kNoAlerts};
constexpr Cond kBear[] = {{StatId::CqcChokes, Op::Ge, 100}};
constexpr Cond kEagleEmblem[] = {{StatId::Headshots, Op::Ge, 150}};
constexpr Cond kAssassin[] = {{StatId::KnifeKills, Op::Ge, 50},
                               {StatId::CqcChokes, Op::Ge, 50},
                               {StatId::Alerts, Op::Lt, 25}};
constexpr Cond kPigeonMgs4[] = {kNoKills};
constexpr Cond kBlueBird[] = {{StatId::ItemsGiven, Op::Gt, 50}};
constexpr Cond kHawkEmblem[] = {{StatId::MilitiaPraise, Op::Ge, 25}};
constexpr Cond kLittleGray[] = {{StatId::WeaponsOwned, Op::Ge, 69}};
constexpr Cond kAnt[] = {{StatId::BodySearches, Op::Ge, 50}};
constexpr Cond kGibbon[] = {{StatId::HoldUps, Op::Ge, 50}};
constexpr Cond kTortoiseMgs4[] = {{StatId::BoxHours, Op::Ge, 1}};
constexpr Cond kBee[] = {{StatId::ScanPlugUses, Op::Ge, 50}};
constexpr Cond kGecko[] = {{StatId::WallHours, Op::Ge, 1}};
constexpr Cond kScarab[] = {{StatId::SideRolls, Op::Ge, 200}};
constexpr Cond kFrog[] = {{StatId::ForwardRolls, Op::Ge, 100}};
constexpr Cond kInchWorm[] = {{StatId::CrawlHours, Op::Ge, 1}};
constexpr Cond kLobster[] = {{StatId::CrouchHours, Op::Ge, 2.5}};
constexpr Cond kHyena[] = {{StatId::WeaponsPickedUp, Op::Ge, 400}};
constexpr Cond kHog[] = {{StatId::CombatHighs, Op::Ge, 10}};
constexpr Cond kPigMgs4[] = {{StatId::RationsUsed, Op::Ge, 40}};
constexpr Cond kCowMgs4[] = {{StatId::Alerts, Op::Ge, 100}};
constexpr Cond kCrocodileMgs4[] = {{StatId::Kills, Op::Ge, 400}};

constexpr Cond kScorpFam[] = {{StatId::Alerts, Op::Lt, 75},
                               {StatId::Kills, Op::Lt, 250},
                               {StatId::Continues, Op::Lt, 25}};
constexpr Cond kTarantulaFam[] = {{StatId::Alerts, Op::Lt, 75},
                                   {StatId::Kills, Op::Gt, 250},
                                   {StatId::Continues, Op::Lt, 25}};
constexpr Cond kCentipedeFam[] = {{StatId::Alerts, Op::Lt, 75},
                                   {StatId::Kills, Op::Lt, 250},
                                   {StatId::Continues, Op::Gt, 25}};
constexpr Cond kSpiderFam[] = {{StatId::Alerts, Op::Lt, 75},
                                {StatId::Kills, Op::Gt, 250},
                                {StatId::Continues, Op::Gt, 25}};
constexpr Cond kJaguarFam[] = {{StatId::Alerts, Op::Gt, 75},
                                {StatId::Kills, Op::Lt, 250},
                                {StatId::Continues, Op::Lt, 25}};
constexpr Cond kPantherFam[] = {{StatId::Alerts, Op::Gt, 75},
                                 {StatId::Kills, Op::Gt, 250},
                                 {StatId::Continues, Op::Lt, 25}};
constexpr Cond kLeopardFam[] = {{StatId::Alerts, Op::Gt, 75},
                                 {StatId::Kills, Op::Lt, 250},
                                 {StatId::Continues, Op::Gt, 25}};
constexpr Cond kPumaFam[] = {{StatId::Alerts, Op::Gt, 75},
                              {StatId::Kills, Op::Gt, 250},
                              {StatId::Continues, Op::Gt, 25}};

const std::array<RankRule, 35> kMgs4Rules{{
    RankRule{"BIG BOSS", kAnyDiff, Kind::Elite, kBB},
    RankRule{"FOXHOUND", kAnyDiff, Kind::Elite, kFoxHound},
    RankRule{"FOX", kAnyDiff, Kind::Elite, kFox},
    RankRule{"HOUND", kAnyDiff, Kind::Elite, kHound},

    RankRule{"Mantis", kAnyDiff, Kind::Elite, kMantis},
    RankRule{"Octopus", kAnyDiff, Kind::Special, kOctopus},

    RankRule{"Bear", kAnyDiff, Kind::Special, kBear},
    RankRule{"Eagle", kAnyDiff, Kind::Special, kEagleEmblem},
    RankRule{"Assassin", kAnyDiff, Kind::Special, kAssassin},
    RankRule{"Pigeon", kAnyDiff, Kind::Special, kPigeonMgs4},
    RankRule{"Blue Bird", kAnyDiff, Kind::Special, kBlueBird},
    RankRule{"Hawk", kAnyDiff, Kind::Special, kHawkEmblem},
    RankRule{"Little Gray", kAnyDiff, Kind::Special, kLittleGray},
    RankRule{"Ant", kAnyDiff, Kind::Special, kAnt},
    RankRule{"Gibbon", kAnyDiff, Kind::Special, kGibbon},
    RankRule{"Tortoise", kAnyDiff, Kind::Special, kTortoiseMgs4},
    RankRule{"Bee", kAnyDiff, Kind::Special, kBee},
    RankRule{"Gecko", kAnyDiff, Kind::Special, kGecko},
    RankRule{"Scarab", kAnyDiff, Kind::Special, kScarab},
    RankRule{"Frog", kAnyDiff, Kind::Special, kFrog},
    RankRule{"Inch Worm", kAnyDiff, Kind::Special, kInchWorm},
    RankRule{"Lobster", kAnyDiff, Kind::Special, kLobster},
    RankRule{"Hyena", kAnyDiff, Kind::Special, kHyena},
    RankRule{"Hog", kAnyDiff, Kind::Special, kHog},
    RankRule{"Pig", kAnyDiff, Kind::Special, kPigMgs4},
    RankRule{"Cow", kAnyDiff, Kind::Special, kCowMgs4},
    RankRule{"Crocodile", kAnyDiff, Kind::Special, kCrocodileMgs4},

    RankRule{"Scorpion", kAnyDiff, Kind::Regular, kScorpFam},
    RankRule{"Tarantula", kAnyDiff, Kind::Regular, kTarantulaFam},
    RankRule{"Centipede", kAnyDiff, Kind::Regular, kCentipedeFam},
    RankRule{"Spider", kAnyDiff, Kind::Regular, kSpiderFam},
    RankRule{"Jaguar", kAnyDiff, Kind::Regular, kJaguarFam},
    RankRule{"Panther", kAnyDiff, Kind::Regular, kPantherFam},
    RankRule{"Leopard", kAnyDiff, Kind::Regular, kLeopardFam},
    RankRule{"Puma", kAnyDiff, Kind::Regular, kPumaFam},
}};

static_assert(std::size(kMgs4Rules) == 35);

} // namespace

std::span<const RankRule> mgs4_rules()
{
    return kMgs4Rules;
}

std::vector<Match> all_matches_mgs4(const GameStats& s)
{
    std::vector<Match> out;
    std::set<std::string_view> seen;
    bool elite_awarded = false;
    for (const RankRule& r : kMgs4Rules) {
        if (!rule_matches(s, r)) {
            continue;
        }
        if (r.kind == Kind::Elite) {
            if (elite_awarded) {
                continue;
            }
            elite_awarded = true;
        }
        if (seen.insert(r.name).second) {
            out.push_back(Match{r.name, r.kind});
        }
    }
    return out;
}

std::vector<ReqStatus> elite_requirements_mgs4(const GameStats& s)
{
    auto row = [&](const char* label, StatId stat, Op op, double limit) {
        return ReqStatus{label, cond_met(s, Cond{stat, op, limit}),
                         stat_value(s, stat), limit,
                         static_cast<uint8_t>(ReqFmt::Count),
                         static_cast<uint8_t>(op)};
    };
    std::vector<ReqStatus> out;
    out.push_back(row("alerts", StatId::Alerts, Op::Eq, 0));
    out.push_back(row("kills", StatId::Kills, Op::Eq, 0));
    out.push_back(row("continues", StatId::Continues, Op::Eq, 0));
    out.push_back(row("recovery items", StatId::RationsUsed, Op::Eq, 0));
    out.push_back(ReqStatus{"special items", !s.special_item_used, 0, 0,
                            static_cast<uint8_t>(ReqFmt::Count),
                            static_cast<uint8_t>(Op::Eq)});
    out.push_back(ReqStatus{"play time",
                            s.play_time_seconds / 3600.0 < 5.0 && s.play_time_seconds > 0,
                            stat_value(s, StatId::PlayTimeHours), 5,
                            static_cast<uint8_t>(ReqFmt::Time),
                            static_cast<uint8_t>(Op::Lt)});
    return out;
}

}
