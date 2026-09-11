#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

namespace bb::mgs2 {

constexpr size_t kDogTagWordCount = 32;

struct DogTag {
    uint16_t id;
    const char* name_2001;
    const char* area;
    uint8_t campaign; // 0 = Tanker, 1 = Plant
    uint8_t difficulty; // Very Easy through Extreme
    bool bluff = false; // enemy.c sets ENE_STATUS_BLUFF for regular guards with country "0".
    const char* name_2002 = nullptr; // Game's dogtag_data second name, used when GM_CONFIG_DOGTAGS_2002 is set.
};

// IDs and metadata from the game's dogtag_data table. ID 394 is an invalid
// duplicate sentinel and is intentionally omitted.
inline constexpr DogTag kDogTags[] = {
    {0, "Olga Gurlukovich", "w00c", 0, 0},
    {1, "Olga Gurlukovich", "w00c", 0, 1},
    {2, "Olga Gurlukovich", "w00c", 0, 2},
    {3, "Olga Gurlukovich", "w00c", 0, 3},
    {4, "Olga Gurlukovich", "w00c", 0, 4},
    {5, "Iroquois Pliskin", "w43a", 1, 0},
    {6, "Meryl Silverburgh", "w43a", 1, 1},
    {7, "Solid Snake", "w43a", 1, 2},
    {8, "Liquid Snake", "w43a", 1, 3},
    {9, "Hideo Kojima", "w43a", 1, 4},
    {10, "Ross E Bowman", "w00a", 0, 0, false, "Josh Spires"},
    {11, "Nicholas M Capone", "w00a", 0, 1, false, "Justin P Verrier"},
    {12, "Kazuya Ikeno", "w00a", 0, 2, false, "Garret L Peters"},
    {13, "Markus A Lindqvist", "w00a", 0, 3, false, "Pawel Adamczyk"},
    {14, "Sanae Shintani", "w00a", 0, 4, true, "Isao Miyamoto"},
    {15, "Abraham Carrillo", "w00a", 0, 0, false, "Tomomi Nasu"},
    {16, "Donal L Gilliland", "w00a", 0, 1, false, "Ryouichi Matsumoto"},
    {17, "Kim K Christensen", "w00a", 0, 2, false, "Sunny Hsu"},
    {18, "Kenichi Takashima", "w00a", 0, 3, false, "Jonathan S Wilson"},
    {19, "Aki Hitachi", "w00a", 0, 4, true, "Matt Holt"},
    {20, "Kaissar Ag Agnouche", "w00a", 0, 0, false, "Stephen M Driver"},
    {21, "Joe T Holdren", "w00a", 0, 1, false, "Michael S Futter"},
    {22, "Larry D Lionberger", "w00a", 0, 2, false, "Pablo Maisuls"},
    {23, "Thiago S Parra", "w00a", 0, 3, false, "Peter A Sattaur"},
    {24, "Mathieu Trepanier", "w00a", 0, 4, false, "Patrick James Torrence"},
    {25, "Arnaud Delaunay", "w00c", 0, 0, false, "Brendan McLeod"},
    {26, "Shaun P Wilson", "w00c", 0, 1, false, "Michael Vogt"},
    {27, "Chul Kwon", "w00c", 0, 2, false, "Samuel B Sandoval"},
    {28, "Chris J Matzdorf", "w00c", 0, 3, false, "Ako Kudou"},
    {29, "Yoshinori Onodera", "w00c", 0, 4, true, "Malcolm W Bullmore"},
    {30, "Hirosuke Moritomo", "w01c", 0, 2, false, "James I Bartelle"},
    {31, "Adriaan B Scholvinck", "w01c", 0, 3, false, "Johan Hana"},
    {32, "Joshua D Casnocha", "w01c", 0, 4, false, "Adam Christopher Vazquez"},
    {33, "Ryoji Makimura", "w01a", 0, 0, true, "Hajme Tusima"},
    {34, "Jeff K Hui", "w01a", 0, 1, false, "Kei Kudou Mon"},
    {35, "Hoshiko Kamata", "w01a", 0, 2, true, "Daniel Kucan"},
    {36, "Julius Jun", "w01a", 0, 3, false, "Dex J Delfrate"},
    {37, "Louis K Stevenson", "w01a", 0, 4, false, "Alexandra Rattay"},
    {38, "Kumi Sato", "w01b", 0, 0, true, "Yuki Hongoh"},
    {39, "Nobuyoshi Nishimura", "w01b", 0, 1, true, "Eetu Holmqvist"},
    {40, "Marco G Brunato", "w01b", 0, 2, false, "Ronan Khim"},
    {41, "Kazuki Nisimura", "w01b", 0, 3, false, "Joseph R Larger"},
    {42, "Michael D Rogers", "w01b", 0, 4, false, "Enrike Vargas Suarez"},
    {43, "Jesus Bibian Jr", "w01b", 0, 2, false, "Masanori Inui"},
    {44, "Zhang Chao", "w01b", 0, 3, false, "Kouji Takemoto"},
    {45, "Gianluca Peruzzo", "w01b", 0, 4, false, "Akihito Oshima"},
    {46, "Kevin S Purvis", "w01f", 0, 0, false, "Takahiro Yura"},
    {47, "Tomomi Kato", "w01f", 0, 1, true, "Matthew W Hunt"},
    {48, "Jordi C Aldea", "w01f", 0, 2, false, "Jeng L Valencia"},
    {49, "Vishal Kapur", "w01f", 0, 3, false, "Mahde M Mansur"},
    {50, "Travis J Lujan", "w01f", 0, 4, false, "Bruce Murphy"},
    {51, "Yasuhiro Miyamoto", "w01f", 0, 0, false, "David P LaFrenier"},
    {52, "Anders E Leiro", "w01f", 0, 1, false, "David W Pelster"},
    {53, "Sadaaki Kaneyoshi", "w01f", 0, 2, true, "Seth T Hay"},
    {54, "Victor A Cruz", "w01f", 0, 3, false, "Sauzando Aran"},
    {55, "Brendan M Randall", "w01f", 0, 4, false, "Patrick F Coley"},
    {56, "Michael C Anthony", "w01f", 0, 0, false, "Eric Bravo"},
    {57, "Skraktus Mercio", "w01f", 0, 1, false, "Takumi Shimada"},
    {58, "Anthony D Callaghan", "w01f", 0, 2, false, "Daniel W Dennison"},
    {59, "Toshio Noguchi", "w01f", 0, 3, true, "Kouhei Yoshizumi"},
    {60, "Chris D Bernd", "w01f", 0, 4, false, "Patrouillault Celine"},
    {61, "Carlos Garci Garcie", "w01d", 0, 0, false, "Miguel Luis Cunha"},
    {62, "Gavin S Nash", "w01d", 0, 1, false, "Cary J Schwartzman"},
    {63, "Craig M Weldon", "w01d", 0, 2, false, "Flavio Camargos"},
    {64, "Celeste D Sauls", "w01d", 0, 3, false, "Hidetosi Suzuki"},
    {65, "Chantelle M Blair", "w01d", 0, 4, false, "Andre Perron"},
    {66, "Danielle E Ford", "w01d", 0, 3, false, "Joelmir Mazon"},
    {67, "Eduard V Fernandez", "w01d", 0, 4, false, "Matthew M Wilcox"},
    {68, "Daizo Shikama", "w01d", 0, 2, true, "Brad R Whitefield"},
    {69, "Jennifer A Mauck", "w01d", 0, 3, false, "Michiel A Hendriksen"},
    {70, "Yoji Shinkawa", "w01d", 0, 4, true, "Miyaji Masayuki"},
    {71, "Mineshi Kimura", "w01d", 0, 0, true, "Eiji Senke"},
    {72, "David S Eastwick", "w01d", 0, 1, false, "Danilo NE Carbone"},
    {73, "Shinta Nojiri", "w01d", 0, 2, true, "James Broome"},
    {74, "Daniel A Olsson", "w01d", 0, 3, false, "Masahiro Takahashi"},
    {75, "Niko Ionixx Horn", "w01d", 0, 4, false, "Guyver M Scott"},
    {76, "Jonathan Hancock", "w03a", 0, 0, false, "Yohsuke Sakaguchi"},
    {77, "Kozaka Kh Henri", "w03a", 0, 1, false, "Adam Lloyd"},
    {78, "Jun Tanaka", "w03a", 0, 2, false, "Hiromasa Watanabe"},
    {79, "Achim Amann", "w03a", 0, 3, false, "Christopher C Osterwald"},
    {80, "Adnan Hadzic", "w03a", 0, 4, false, "Devin P McCourt"},
    {81, "Bryn T Kershaw", "w03a", 0, 0, false, "Jan Friese"},
    {82, "Gackt ", "w03a", 0, 1, false, "David E Palm"},
    {83, "Bernard A Reeves", "w03a", 0, 2, false, "Kieran Keegan"},
    {84, "Sean P Cullen", "w03a", 0, 3, false, "Lee Seung Min"},
    {85, "Shu Tajima", "w03a", 0, 4, false, "Takahiro Hanaoka"},
    {86, "Michael Hurkmans", "w03a", 0, 0, false, "Marius Torsud"},
    {87, "Thomas P Dohm", "w03a", 0, 1, false, "Yuki Aoki"},
    {88, "Evan M Martin", "w03a", 0, 2, false, "Hirokazu Takahashi"},
    {89, "Tommy Blunt", "w03a", 0, 3, false, "TSZ TING"},
    {90, "Stuart J Batchelar", "w03a", 0, 4, false, "JinagXi Konglong"},
    {91, "Ken Ogasawara", "w02a", 0, 0, false, "Andrej Pedercina Mimica"},
    {92, "Enrique Camacho", "w02a", 0, 1, false, "William N Bruce"},
    {93, "Bruno A Montenegro", "w02a", 0, 2, false, "Marwan Abdullah AlHarbi"},
    {94, "David Chau", "w02a", 0, 3, false, "Manabu Nishiyama"},
    {95, "Masataka Nishiyama", "w02a", 0, 4, false, "Yuji Gotoh"},
    {96, "Petro Kyrylenko", "w02a", 0, 0, false, "Mike J Corbitt"},
    {97, "Takashi Ohari", "w02a", 0, 1, false, "Chien Jen Liang"},
    {98, "Almerindo Lemke", "w02a", 0, 2, false, "Henning Rhoden"},
    {99, "Philippe Ah Mouritzen", "w02a", 0, 3, false, "David Heng Liu"},
    {100, "Michael M Wong", "w02a", 0, 4, false, "Tomi S Hakulinen"},
    {101, "Aaron F Kopf", "w02a", 0, 0, false, "Aaron T Powell"},
    {102, "Max C Wood", "w02a", 0, 1, false, "Luke Boulerice"},
    {103, "Satoshi Hirano", "w02a", 0, 2, true, "Marcelo Zamorano"},
    {104, "Yoko Niiyama", "w02a", 0, 3, false, "Wei Zhang"},
    {105, "Manabu Nakamura", "w02a", 0, 4, false, "Suneel Buggal"},
    {106, "John W Fleming", "w02a", 0, 1, false, "Yusuke Sasaki"},
    {107, "Justin C Cumley", "w02a", 0, 2, false, "Simon F Picard"},
    {108, "John V Teves", "w02a", 0, 3, false, "Han Tao"},
    {109, "Simon P Sargent", "w02a", 0, 4, false, "Anibal Rodriguez"},
    {110, "Kristian Lindin", "w02a", 0, 2, false, "Haden K Oneil"},
    {111, "Shintaro Naka", "w02a", 0, 3, true, "Takashi Shiohama"},
    {112, "Yosuke Kamezaki", "w02a", 0, 4, false, "Fahim Mumin"},
    {113, "James P Fitzgibbons", "w02a", 0, 0, false, "Jeff A Saylor Jr"},
    {114, "Edward B Elston", "w02a", 0, 1, false, "Travis J Long"},
    {115, "Mika Abe", "w02a", 0, 2, false, "Yuji Higuchi"},
    {116, "Natsuyo Tanaka", "w02a", 0, 3, true, "James C Layton"},
    {117, "Clarke A Baldwi", "w02a", 0, 4, false, "Nicholas A Kowalcyk"},
    {118, "Alexander Strigl", "w02a", 0, 0, false, "Kazuki Tokunaga"},
    {119, "Christopher D Dadah", "w02a", 0, 1, false, "Steven A Weekes"},
    {120, "Tatsuya Takada", "w02a", 0, 2, false, "Husain Abdulrazaq AlHasan"},
    {121, "Yuji Korekado", "w02a", 0, 3, true, "Jesus Auron Noatsuna"},
    {122, "Hideki Sasaki", "w02a", 0, 4, true, "Adam M Conrad"},
    {123, "Hiro Takada", "w11a", 1, 0, false, "Brad I Lanning"},
    {124, "Thomas G Cardner", "w11a", 1, 1, false, "Emiko Yokoshima"},
    {125, "Clinton J Heileman Jr", "w11a", 1, 0, false, "Charles P Pinkerton"},
    {126, "Matthew R Vogel", "w11a", 1, 1, false, "Terry Cheung"},
    {127, "Mike J Newman", "w12a", 1, 0, false, "Jason M Gillispie"},
    {128, "Megumi Nakaniihara", "w12a", 1, 1, false, "Jacqueline D Benzon"},
    {129, "Sotaro Tojima", "w12a", 1, 2, true, "Jordan A Bowman"},
    {130, "Yamato Hagiwara", "w12a", 1, 3, false, "Ihor Novosilets"},
    {131, "Takeshi Sato", "w12a", 1, 4, false, "Makoto Shiragaki"},
    {132, "Rayyan A Said", "w12b", 1, 0, false, "Samuel Andi Hendranata"},
    {133, "Matthew A Bullock", "w12b", 1, 1, false, "Akihide Yonekura"},
    {134, "Christophe L Lallemand", "w12b", 1, 2, false, "Sean M Culhane"},
    {135, "Chris Walker", "w12b", 1, 3, false, "Julian Tan"},
    {136, "Shinji Yamashita", "w12b", 1, 4, true, "Richard W Hair Jr"},
    {137, "Shigeo Okajima", "w12b", 1, 0, true, "Yasuhiro Matsuzaki"},
    {138, "Addam J Drew", "w12b", 1, 1, false, "Goushi Kawamura"},
    {139, "Hurell F Lyons", "w12b", 1, 2, false, "Jamie DR Dickie"},
    {140, "Brian D Hagermann", "w12b", 1, 3, false, "Max K Do"},
    {141, "Ryan T Cronkright", "w12b", 1, 4, false, "Spencer P Gallagher"},
    {142, "Cord B Smith", "w13a", 1, 0, false, "Artthapong Siriamonthep"},
    {143, "Yusuke Takada", "w13a", 1, 1, false, "William R Kelly"},
    {144, "Juntaro Saito", "w13a", 1, 2, true, "Daichi Baba"},
    {145, "Makoto Sonoyama", "w13a", 1, 3, true, "Jenam Ryu"},
    {146, "Josef Karsch", "w13a", 1, 4, false, "Ruben Alexander van Ophuizen"},
    {147, "Mario C Lopez", "w13a", 1, 0, false, "Amadeus Knothe"},
    {148, "Miles D Ashley", "w13a", 1, 1, false, "Ryosuke Sugie"},
    {149, "So Toyota", "w13a", 1, 2, true, "David L Newell"},
    {150, "Christoph C Reinicke", "w13a", 1, 3, false, "Kit M Paines"},
    {151, "Emmanuel Y L Passian", "w13a", 1, 4, false, "Leevi Mursula"},
    {152, "Shuhei Tanaka", "w14a", 1, 0, true, "Bob Fu"},
    {153, "Mark E Francis", "w14a", 1, 1, false, "Masanori Masabu Ihara"},
    {154, "Robert J Bryk", "w14a", 1, 2, false, "Bren J Fraher"},
    {155, "Justin D Ebersole", "w14a", 1, 3, false, "Stephen M Stretton"},
    {156, "Lee M Mccowen", "w14a", 1, 4, false, "Roy Grizzly"},
    {157, "Momoko Kawai", "w14a", 1, 0, false, "Yasuo Suzuki"},
    {158, "Kazuki Muraoka", "w14a", 1, 1, true, "Hiroyuki Doi"},
    {159, "Scott A Morgan", "w14a", 1, 2, false, "Gavin C Elliott"},
    {160, "William A Catacutan", "w14a", 1, 3, false, "Adrian Styrsky Ellwood"},
    {161, "Hironobu Matsui", "w14a", 1, 4, true, "Murat Sahiner"},
    {162, "Kengo Iwata", "w15a", 1, 0, false, "Yiqiang Yao"},
    {163, "Tom A Hutchinson", "w15a", 1, 1, false, "Marius Alain Nguyen"},
    {164, "George T Joseph", "w15a", 1, 2, false, "Johnny Chong"},
    {165, "Natalie Yip", "w15a", 1, 3, false, "Hanbum Son"},
    {166, "Drew J Elmer", "w15a", 1, 4, false, "Casey K Chan"},
    {167, "Daniel Modol", "w16a", 1, 0, false, "Vanessa E Mejia"},
    {168, "Corey E Louden", "w16a", 1, 1, false, "Kevin Chen"},
    {169, "Vahe V Varujan", "w16a", 1, 2, false, "Chuoc Shan Lam"},
    {170, "Kelsy L Clark", "w16a", 1, 3, false, "Juan Luis Ruiz"},
    {171, "Toru Kawakami", "w16a", 1, 4, true, "David Yu"},
    {172, "Noriyuki Katsumura", "w16a", 1, 0, true, "AnthonyJ Willreign Supreme"},
    {173, "Ian J Andrews", "w16a", 1, 1, false, "Boudraa Reda"},
    {174, "Mark Mugendi", "w16a", 1, 2, false, "Markus Huendgen"},
    {175, "Bjoern Heide", "w16a", 1, 3, false, "Takayuki Kawakubo "},
    {176, "Adam J Sarpolis", "w16a", 1, 4, false, "Yuta Shimizu"},
    {177, "Tetsuro Sueyoshi", "w17a", 1, 0, false, "Marques Dean"},
    {178, "Tim J Veldboom", "w17a", 1, 1, false, "Yu Ya Nan"},
    {179, "Irene C Carvalho", "w17a", 1, 2, false, "Michael Roger Aarons"},
    {180, "Daniel Y Kato", "w17a", 1, 3, false, "Gregory M Roy"},
    {181, "Jyunpei Hirano", "w17a", 1, 4, false, "Callum G Whitehurst "},
    {182, "Tony J Ylaranta", "w17a", 1, 0, false, "Kevin M Miller"},
    {183, "David C Ratanaseangsuang", "w17a", 1, 1, false, "Takayuki Ueda"},
    {184, "Jools Watsham", "w17a", 1, 2, false, "Takuji Tada"},
    {185, "Alexandre Reis Cunha Dantas", "w17a", 1, 3, false, "Randy Lee Way"},
    {186, "Kyle S Carrigan", "w17a", 1, 4, false, "Matthew T Fenelon"},
    {187, "Adam J Schick", "w18a", 1, 2, false, "Joseph A McClaren"},
    {188, "Jason Enos", "w18a", 1, 3, false, "Taggart D Ryan"},
    {189, "Matt J Van Leeuwen", "w18a", 1, 4, false, "Edward Evans"},
    {190, "Paul R Martin", "w18a", 1, 0, false, "Mathias Ellemann Jensen"},
    {191, "Shiro Mukaide", "w18a", 1, 1, true, "Luis Silva Garcia"},
    {192, "Takayoshi Ogawa", "w18a", 1, 2, false, "Dustin Weger"},
    {193, "Josiah F Thorne", "w18a", 1, 3, false, "Sunny Hsu"},
    {194, "Matthew B Boyett", "w18a", 1, 4, false, "Sonny Skinner"},
    {195, "Leandro M Cardoso", "w18a", 1, 0, false, "Christian Ryan De Ramos"},
    {196, "Rie Nishiki", "w18a", 1, 1, true, "Brian A Brown"},
    {197, "Jamie A Trumper", "w18a", 1, 2, false, "Wojtek Kujawa"},
    {198, "Yoshikazu Matsuhana", "w18a", 1, 3, true, "Sascha Norbert Behr"},
    {199, "Collis R Williams", "w18a", 1, 4, false, "Ryo Hanzawa"},
    {200, "Claudia Cd Diessner", "w18a", 1, 3, false, "Derrick S Edick"},
    {201, "Nobumitsu Tanaka", "w18a", 1, 4, true, "Mixalis Tsakiris"},
    {202, "Al J Josef", "w19a", 1, 0, false, "Derek Broadbent"},
    {203, "Mariko Nakamura", "w19a", 1, 1, false, "Nicholas S DeGrazia"},
    {204, "Sergio Carranza", "w19a", 1, 2, false, "Kenji Yamaguchi"},
    {205, "Hiroyuki Tsuchida", "w19a", 1, 3, true, "Michael Mantilla"},
    {206, "Matthew C Miller", "w19a", 1, 4, false, "Wayne A Hindman II"},
    {207, "Kyoko Hariyama", "w19a", 1, 0, true, "Till Schlegel"},
    {208, "Emily Britt", "w19a", 1, 1, false, "Yuuki Ito"},
    {209, "Michiko Arai", "w19a", 1, 2, false, "John A McCormick"},
    {210, "Devan V Tailor", "w19a", 1, 3, false, "Eiichiro Kimura"},
    {211, "Masashi Watanabe", "w19a", 1, 4, true, "Zeeshan Raza"},
    {212, "Barna K Olvedi", "w20a", 1, 0, false, "Nana Mizuki"},
    {213, "Daniel A Longworth", "w20a", 1, 1, false, "Anmar Saad Albinyan"},
    {214, "Takashi Mizutani", "w20a", 1, 2, true, "Robert Viset"},
    {215, "SCOTT DOLPH", "w20a", 1, 3, false, "Omar Khettab"},
    {216, "Yoshiyuki Koido", "w20a", 1, 4, true, "Mikko Pellervo Rekola"},
    {217, "Yuta Kunibe", "w20a", 1, 0, true, "Csaba Millei"},
    {218, "Caroline M L Gibson", "w20a", 1, 1, false, "Steve Masao Terada"},
    {219, "Christian Cr Renner", "w20a", 1, 2, false, "Tommi Eklof"},
    {220, "Ikuya Nakamura", "w20a", 1, 3, true, "Ting Kae Lung"},
    {221, "Tsunehiko Shibata", "w20a", 1, 4, true, "Kouji Yamashita"},
    {222, "Andy B Gilder", "w20a", 1, 2, false, "Gary L Curtis"},
    {223, "Takaaki Kitamura", "w20a", 1, 3, true, "Eric J Melen"},
    {224, "Ryan C Sheffer", "w20a", 1, 4, false, "Makoto Chiba"},
    {225, "Ryoko Yoshimura", "w20a", 1, 3, true, "Chad P Bala"},
    {226, "Carlos X Luna", "w20a", 1, 4, false, "David Gomez"},
    {227, "David A Ginepri", "w20a", 1, 4, false, "Christopher R Martinez"},
    {228, "Daniel C Bell", "w20b", 1, 0, false, "Jordan J Davis"},
    {229, "Yuki Sawada", "w20b", 1, 1, false, "Yuuki Yagi"},
    {230, "Takashi Horikawa", "w20b", 1, 2, false, "Court Stauff"},
    {231, "Yoshiteru Kobayashi", "w20b", 1, 3, true, "Gustavo L Lazo"},
    {232, "Emmanuel Phung", "w20b", 1, 4, false, "Masayuki Yabuki"},
    {233, "Takahiro Omori", "w20b", 1, 0, true, "Luke Christian Roberts"},
    {234, "Daijiro Takeshima", "w20b", 1, 1, false, "Brigitte Dickenscheid"},
    {235, "Kevin T Petty", "w20b", 1, 2, false, "Frode A Steine"},
    {236, "Chris M Flohr", "w20b", 1, 3, false, "Alan Chon"},
    {237, "Ryosaku Ueno", "w20b", 1, 4, true, "Brendan OConnor"},
    {238, "Guilherme K Saran", "w20b", 1, 2, false, "Katharine Irene Hargrove"},
    {239, "Nicholas J Schreiber", "w20b", 1, 3, false, "Christian Alexander Heinzer"},
    {240, "Alan J Harries", "w20b", 1, 4, false, "Grant Drain"},
    {241, "Nadim Daban", "w22a", 1, 0, false, "Sinya Tusunoda"},
    {242, "Masatoshi Uehara", "w22a", 1, 1, true, "Agmall Sarwari"},
    {243, "Chen Yen Wen", "w22a", 1, 2, false, "David R Papps"},
    {244, "Kenichiro Kano", "w22a", 1, 3, true, "Daisuke Kato"},
    {245, "Marco O Scherrer", "w22a", 1, 4, false, "Yoshihiro Sakai"},
    {246, "Timothy J Kane", "w22a", 1, 0, false, "Asia R Pickle"},
    {247, "Ikki Sakakibara", "w22a", 1, 1, true, "Naoto Yamazaki"},
    {248, "Caroline Frechette", "w22a", 1, 2, false, "Matt D Huffman"},
    {249, "Hiroki Satoyoshi", "w22a", 1, 3, true, "Takeshi Umeoka"},
    {250, "Edmond V To", "w22a", 1, 4, false, "Greg T McDaniel"},
    {251, "Juergen Jur Goessnitzer", "w22a", 1, 1, false, "Terry Matalas"},
    {252, "Daisuke Mogi", "w22a", 1, 2, true, "Ali H Rafati"},
    {253, "Shuyo Murata", "w22a", 1, 3, true, "Masato Aoyama"},
    {254, "Chevrinais Thomas", "w22a", 1, 4, false, "Ikuo Uchiumi"},
    {255, "Futoshi Satou", "w22a", 1, 3, false, "Yo Narita"},
    {256, "Hiroshi Yokote", "w22a", 1, 4, false, "Ai Shimamatu"},
    {257, "Berl B Pottstam", "w22a", 1, 4, false, "Derek Faria"},
    {258, "Allen J Chang", "w23a", 1, 0, false, "Daniel P Wells"},
    {259, "Yuko Yano", "w23a", 1, 1, true, "Sefie J Nolte"},
    {260, "Tomokazu Fukushima", "w23a", 1, 2, true, "Mais M Baali"},
    {261, "Shinpei Murakami", "w23a", 1, 3, true, "Michael S Chang"},
    {262, "Kenichiro Shigeno", "w23a", 1, 4, true, "Artmed S Hernandez"},
    {263, "Jean Luc Cougar", "w24a", 1, 0, false, "David Anthony Wilson"},
    {264, "Rick Naylor", "w24a", 1, 1, false, "David Li-San Chen"},
    {265, "Monte S Tate", "w24a", 1, 2, false, "Satoru Oda"},
    {266, "David A Lesslie", "w24a", 1, 3, false, "Lucky Zhu"},
    {267, "Kaori Yamada", "w24a", 1, 4, false, "Rhys J Perkins"},
    {268, "Luis A Fernandes", "w24a", 1, 0, false, "Jasper A Hefko"},
    {269, "Jeremy A Davis", "w24a", 1, 1, false, "Ginseng W Mileur"},
    {270, "Stephanie Hattenberger ", "w24a", 1, 2, false, "Kamal Aggarwal"},
    {271, "Brian R Strack", "w24a", 1, 3, false, "Ryo Nitta"},
    {272, "Ryan J Crane", "w24a", 1, 4, false, "Henry Van"},
    {273, "Yuta Kiguchi", "w24a", 1, 0, false, "Steven L Harper"},
    {274, "Zephan G Kirkpatrick", "w24a", 1, 1, false, "Remy Schuster"},
    {275, "Koichi Nakano", "w24a", 1, 2, true, "Anthony Hampton"},
    {276, "Skip M Murray", "w24a", 1, 3, false, "Shunsuke Katoh"},
    {277, "Gareth J Lewis", "w24a", 1, 4, false, "Takashi Saitou"},
    {278, "Matt T Federspiel", "w24a", 1, 2, false, "Ivan Lui"},
    {279, "Axel R Zijderveld", "w24a", 1, 3, false, "Toru Yamagishi"},
    {280, "Andreas R Ramsauer", "w24a", 1, 4, false, "Nicholas G Benson"},
    {281, "Masafumi Okuta", "w24b", 1, 0, true, "Yasuhiko Nakamura"},
    {282, "Kenneth Wong", "w24b", 1, 1, false, "Matthew J Whitney"},
    {283, "Sachiko Hara", "w24b", 1, 2, true, "Hideaki Dei"},
    {284, "Kamran Keenan", "w24b", 1, 3, false, "Sandra Stevic"},
    {285, "Justin A Cagle", "w24b", 1, 4, false, "Hongoh Yuki"},
    {286, "Matthew R Bartz", "w24b", 1, 0, false, "Chris Escobar"},
    {287, "Michael D Craft", "w24b", 1, 1, false, "Ricardo Enrique Salazar"},
    {288, "Ray A Holdren", "w24b", 1, 2, false, "Cesar Pariona Oncebay"},
    {289, "Tomonori Morita", "w24b", 1, 3, true, "Brady N Hartel"},
    {290, "Christopher S Korte", "w24b", 1, 4, false, "Nicholas A Fuentes"},
    {291, "Kunio Takabe", "w24b", 1, 0, true, "Elliott l Kugler"},
    {292, "Stephen D Haynes", "w24b", 1, 1, false, "Randall J Koerte"},
    {293, "Carlos I Siu", "w24b", 1, 2, false, "Yuuko Hayashi"},
    {294, "Julien Jd Dort", "w24b", 1, 3, false, "Daichi Sumitomo"},
    {295, "Masahiro Yoshinaga", "w24b", 1, 4, true, "Roy PC Smillie"},
    {296, "Joey Simkins", "w24d", 1, 0, false, "Mai Ohta"},
    {297, "Joshua A Crandall", "w24d", 1, 1, false, "Syouhei Yoshino"},
    {298, "Eric G Macway", "w24d", 1, 2, false, "Viktor Hamrefors"},
    {299, "Steven Schmitt", "w24d", 1, 3, false, "Chris David Adlam"},
    {300, "Daisuke Nishimura", "w24d", 1, 4, false, "Chris S Austin"},
    {301, "Sve G Westli", "w24d", 1, 0, false, "Roth W Lanphear"},
    {302, "Hiroyuki Inoue", "w24d", 1, 1, true, "Mika M Claudepierre"},
    {303, "Ian J Roberts", "w24d", 1, 2, false, "Miroku Sato"},
    {304, "Michael O Kress", "w24d", 1, 3, false, "Shingo Minakawa"},
    {305, "Viana Siles Mauricette", "w24d", 1, 4, false, "Tony Pawlik"},
    {306, "Sam M Shrimpton", "w24d", 1, 0, false, "Cheng exs Cheng"},
    {307, "Andrew J Walker", "w24d", 1, 1, false, "Joshua M Busby"},
    {308, "Alexandre Bertrand", "w24d", 1, 2, false, "Kojiro Hayama"},
    {309, "Stephane Tudela", "w24d", 1, 3, false, "Victor Sjostrom"},
    {310, "Chen Yung Kok", "w24d", 1, 4, false, "Erik PO Appelblad"},
    {311, "Norihiko Hibino", "w24d", 1, 0, true, "Liam P Slater"},
    {312, "Kazunobu Uehara", "w24d", 1, 1, true, "Oliver Swann"},
    {313, "Peter D Mccarthy", "w24d", 1, 2, false, "Mark S Eaton Fry"},
    {314, "Yoriko Shimizu", "w24d", 1, 3, true, "Hiroyuki Sakashita"},
    {315, "Anthony J Barritt", "w24d", 1, 4, false, "Tooru Morita"},
    {316, "Erik R Christy", "w25d", 1, 0, false, "Neill W Perry"},
    {317, "Thomas Szedlak", "w25d", 1, 1, false, "Toshifumi Kotogi"},
    {318, "Ichiro Kutome", "w25d", 1, 2, true, "Han Yi Cheng"},
    {319, "Yun-Ho Kim", "w25d", 1, 3, false, "Kazuto Ikeda"},
    {320, "Kaori Yae", "w25d", 1, 4, false, "John Guest Jr"},
    {321, "Jason C Patino", "w28a", 1, 0, false, "Robert W Goodenow III"},
    {322, "Shuichi Hata", "w28a", 1, 1, false, "Erich Werdermann"},
    {323, "Yutaka Negishi", "w28a", 1, 2, true, "Yuta Tsubasa Teruya"},
    {324, "Jun Sukegawa", "w28a", 1, 3, false, "Victor Melnik"},
    {325, "Isao A Sato", "w28a", 1, 4, false, "Sunny Lee"},
    {326, "Maarten Van Der Zwan", "w28a", 1, 0, false, "Mathias Buntrock"},
    {327, "Frank A Morales", "w28a", 1, 1, false, "Kouhei Tuzita"},
    {328, "Renata N Csio", "w28a", 1, 2, false, "Mikiya Horiuchi"},
    {329, "Joey P Gonzales", "w28a", 1, 3, false, "Thiago Monezi Pires de Avila"},
    {330, "Ryan J Schettle", "w28a", 1, 4, false, "Lei Adnan"},
    {331, "Motoyuki Yoshioka", "w31a", 1, 0, true, "Andrei V Galinski"},
    {332, "Lee P French", "w31a", 1, 1, false, "Federico Biscetti"},
    {333, "Tony J Case", "w31a", 1, 2, false, "Martin Johansson"},
    {334, "Christopher Heck", "w31a", 1, 3, false, "Phil A Marchant"},
    {335, "Andy Lam", "w31a", 1, 4, false, "Ryo Harada"},
    {336, "Adrian Thien", "w31a", 1, 0, false, "Naoki Inoue"},
    {337, "Marcos A Gomez", "w31a", 1, 1, false, "Matt R Miller"},
    {338, "Paul M Blacketer", "w31a", 1, 2, false, "TaRo ArAkAwA"},
    {339, "Martin Kukowka", "w31a", 1, 3, false, "Yuusuke Kitamura"},
    {340, "Hiroaki Yoshiike", "w31a", 1, 4, true, "Hugo L Vidal"},
    {341, "Yuki Higuchi", "w31a", 1, 0, false, "Hiu Fung Wong"},
    {342, "Scott K Cleary", "w31a", 1, 1, false, "Jean Pierre Laforce"},
    {343, "Dennis J Krimpelbein", "w31a", 1, 2, false, "Gaku Yamada"},
    {344, "Alex N Martinez", "w31a", 1, 3, false, "Carlos C Cresswell"},
    {345, "Cedric Krolikowski", "w31a", 1, 4, false, "Matthias Sauermann"},
    {346, "Stanley A Garcia", "w31a", 1, 2, false, "Zheng Xuan "},
    {347, "Satoru Kobayashi", "w31a", 1, 3, true, "Amir C Saya"},
    {348, "Sebastian J Pitman", "w31a", 1, 4, false, "Rick BigBan Laiso"},
    {349, "Charles P Quivers", "w31a", 1, 2, false, "Erasmo O Metos"},
    {350, "Giovanni Cavalliere", "w31a", 1, 3, false, "Jeffrey Fong"},
    {351, "Mark A Matuszewski", "w31a", 1, 4, false, "Josh Garcia"},
    {352, "Sevak N Fair", "w31a", 1, 2, false, "Florian Busch"},
    {353, "Gabriel Freitas Peres", "w31a", 1, 3, false, "Sverrir Fridriksson"},
    {354, "Xavier R Garcia", "w31a", 1, 4, false, "Clayton R Worrell"},
    {355, "Peter Stillman", "w20c", 1, 0},
    {356, "Peter Stillman", "w20c", 1, 1},
    {357, "Peter Stillman", "w20c", 1, 2},
    {358, "Peter Stillman", "w20c", 1, 3},
    {359, "Peter Stillman", "w20c", 1, 4},
    {360, "Dave Cox", "w04a", 0, 0, false, "Kamil Mania"},
    {361, "Rodrigo Spinetti", "w04a", 0, 1, false, "Marco Silvano"},
    {362, "Pawel Majewski", "w04a", 0, 2, false, "Robert R Bruce"},
    {363, "Bryan D Scheibe", "w04a", 0, 3, false, "Ha Su Jung"},
    {364, "Ho Yeung Tsang", "w04a", 0, 4, false, "David Oh"},
    {365, "Cory A Noll", "w04a", 0, 0, false, "Ismael Miranda de Andrade"},
    {366, "Frank Gther", "w04a", 0, 1, false, "Xesk Malone"},
    {367, "Mark W Bruce", "w04a", 0, 2, false, "Kenzi Tanda"},
    {368, "Yukho Wong", "w04a", 0, 3, false, "Minako Nakazawa"},
    {369, "Ulf T Lundh", "w04a", 0, 4, false, "Shinichi Furusho"},
    {370, "Youssef Fassi-Fihri", "w04a", 0, 2, false, "Ivan E Saldarriaga"},
    {371, "Lars Crama", "w04a", 0, 3, false, "Kel M Booker"},
    {372, "Christian Nordstr", "w04b", 0, 2, false, "Christopher G Hercus"},
    {373, "Jonathan Murphy", "w04b", 0, 3, false, "Asif A Ali"},
    {374, "Christopher J Uzdanovich", "w04b", 0, 4, false, "Daisuke mercury Shimada"},
    {375, "Hiro Miyajima", "w04b", 0, 2, false, "Tomohiro Katoh"},
    {376, "Gary K Yong", "w04b", 0, 3, false, "Tatsumi Mizuguchi"},
    {377, "Alex C Wilson", "w04b", 0, 4, false, "Phil A Marchant"},
    {378, "Andreas Ebeler", "w04c", 0, 0, false, "Darryl F Kemp"},
    {379, "James N Janovsky", "w04c", 0, 1, false, "Michalis Taubert"},
    {380, "Nathaniel Lord", "w04c", 0, 2, false, "David D Crumpler"},
    {381, "Yuki Miyata", "w04c", 0, 3, false, "Ali Karar"},
    {382, "Michael A Hare", "w04c", 0, 4, false, "Landon F Hilde"},
    {383, "Gary J Davidson", "w04c", 0, 0, false, "Yoshitoshi Toyoda"},
    {384, "Abigail G Sanchez", "w04c", 0, 1, false, "Lee A Myers"},
    {385, "Carlos Kiho", "w04c", 0, 2, false, "Joao E Martins"},
    {386, "Evan A Ball", "w04c", 0, 3, false, "Benjamin Phoon"},
    {387, "Andrew N Bartlett", "w04c", 0, 4, false, "Jeremy J Drake"},
    {388, "Rafael Estaregue", "w04c", 0, 1, false, "Rock Young"},
    {389, "Tim U Chan", "w04c", 0, 2, false, "Mika A Taavela"},
    {390, "Jason B Wray", "w04c", 0, 3, false, "Jason L Hines"},
    {391, "Iiro Karvinen", "w04c", 0, 4, false, "Fred A Thiele"},
    {392, "Andrew J Baker", "w04c", 0, 3, false, "Kurt W Bartholomew"},
    {393, "Marcin A Cieslinski", "w04c", 0, 4, false, "Nagisa Kase"},
};

// First tag ID for each normal area; boss/special tags join their area's group.
inline constexpr const char* kDogTagAreas[] = {
    "w00a", "w00c", "w01c", "w01a", "w01b", "w01f", "w01d", "w03a", "w02a",
    "w11a", "w12a", "w12b", "w13a", "w14a", "w15a", "w16a", "w17a", "w18a",
    "w19a", "w20a", "w20b", "w22a", "w23a", "w24a", "w24b", "w24d", "w25d",
    "w28a", "w31a", "w20c", "w04a", "w04b", "w04c", "w43a",
};

constexpr bool dog_tag_collected(const uint32_t* flags, size_t id)
{
    return id < kDogTagWordCount * 32
        && (flags[id / 32] & (uint32_t{1} << (id % 32))) != 0;
}

constexpr int dog_tag_count(const uint32_t* flags)
{
    int count = 0;
    for (size_t i = 0; i < kDogTagWordCount; ++i) {
        count += std::popcount(flags[i]);
    }
    return count;
}

constexpr bool dog_tag_available(const DogTag& tag, int mission, uint8_t difficulty)
{
    const uint8_t roster = difficulty > 4 ? 4 : difficulty;
    const uint8_t campaign = mission == 16 ? 0 : 1;
    return tag.difficulty == roster && (mission == 32 || tag.campaign == campaign);
}

constexpr const char* dog_tag_name(const DogTag& tag, bool use_2002)
{
    return use_2002 && tag.name_2002 ? tag.name_2002 : tag.name_2001;
}

// Story/flag gates for tags that are not available for the whole run. Story
// thresholds are values of the game's ST enum; flag offsets are byte offsets
// into the GCL variable buffer (see variable.sym in the HD source).
enum class DogTagGateKind : uint8_t { After, Until };
enum class DogTagGateSource : uint8_t { Story, Flag };

struct DogTagFlag {
    uint16_t offset;
    uint8_t bit;
};

inline constexpr DogTagFlag kDogTagFlags[] = {
    {0x0D1E, 7}, // w02a falling soldier seen
    {0x0C6F, 7}, // w02a door repair event done
    {0x1062, 3}, // w24d exercise soldier seen
    {0x04C4, 1}, // w01b shadow found
    {0x0C42, 6}, // global right watertight door opened
    {0x036D, 6}, // Olga gone
    {0x09B2, 0}, // w45a Tengu appeared
};

struct DogTagGate {
    uint16_t id;
    DogTagGateKind kind;
    DogTagGateSource source;
    uint16_t value; // p_story threshold, or index into kDogTagFlags
    const char* trigger;
};

inline constexpr DogTagGate kDogTagGates[] = {
    {207, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {208, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {209, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {210, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {211, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {182, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {183, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {184, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {185, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {186, DogTagGateKind::After, DogTagGateSource::Story, 122, "Fatman"},
    {212, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {213, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {214, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {215, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {216, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {219, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {220, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {221, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {223, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {224, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {226, DogTagGateKind::Until, DogTagGateSource::Story, 323, "Vamp sniper"},
    {217, DogTagGateKind::After, DogTagGateSource::Story, 323, "Vamp sniper"},
    {218, DogTagGateKind::After, DogTagGateSource::Story, 323, "Vamp sniper"},
    {222, DogTagGateKind::After, DogTagGateSource::Story, 323, "Vamp sniper"},
    {225, DogTagGateKind::After, DogTagGateSource::Story, 323, "Vamp sniper"},
    {227, DogTagGateKind::After, DogTagGateSource::Story, 323, "Vamp sniper"},
    {255, DogTagGateKind::After, DogTagGateSource::Story, 92, "Peter"},
    {256, DogTagGateKind::After, DogTagGateSource::Story, 92, "Peter"},
    {113, DogTagGateKind::Until, DogTagGateSource::Flag, 0, "falling soldier"},
    {114, DogTagGateKind::Until, DogTagGateSource::Flag, 0, "falling soldier"},
    {115, DogTagGateKind::Until, DogTagGateSource::Flag, 0, "falling soldier"},
    {116, DogTagGateKind::Until, DogTagGateSource::Flag, 0, "falling soldier"},
    {117, DogTagGateKind::Until, DogTagGateSource::Flag, 0, "falling soldier"},
    {118, DogTagGateKind::Until, DogTagGateSource::Flag, 1, "door repair"},
    {119, DogTagGateKind::Until, DogTagGateSource::Flag, 1, "door repair"},
    {120, DogTagGateKind::Until, DogTagGateSource::Flag, 1, "door repair"},
    {121, DogTagGateKind::Until, DogTagGateSource::Flag, 1, "door repair"},
    {122, DogTagGateKind::Until, DogTagGateSource::Flag, 1, "door repair"},
    {311, DogTagGateKind::Until, DogTagGateSource::Flag, 2, "exercise soldier"},
    {312, DogTagGateKind::Until, DogTagGateSource::Flag, 2, "exercise soldier"},
    {313, DogTagGateKind::Until, DogTagGateSource::Flag, 2, "exercise soldier"},
    {314, DogTagGateKind::Until, DogTagGateSource::Flag, 2, "exercise soldier"},
    {315, DogTagGateKind::Until, DogTagGateSource::Flag, 2, "exercise soldier"},
    {38, DogTagGateKind::Until, DogTagGateSource::Flag, 3, "shadow"},
    {39, DogTagGateKind::Until, DogTagGateSource::Flag, 3, "shadow"},
    {40, DogTagGateKind::Until, DogTagGateSource::Flag, 3, "shadow"},
    {41, DogTagGateKind::Until, DogTagGateSource::Flag, 3, "shadow"},
    {42, DogTagGateKind::Until, DogTagGateSource::Flag, 3, "shadow"},
    {25, DogTagGateKind::Until, DogTagGateSource::Flag, 4, "right door"},
    {26, DogTagGateKind::Until, DogTagGateSource::Flag, 4, "right door"},
    {27, DogTagGateKind::Until, DogTagGateSource::Flag, 4, "right door"},
    {28, DogTagGateKind::Until, DogTagGateSource::Flag, 4, "right door"},
    {29, DogTagGateKind::Until, DogTagGateSource::Flag, 4, "right door"},
    {0, DogTagGateKind::Until, DogTagGateSource::Flag, 5, "Olga leaves"},
    {1, DogTagGateKind::Until, DogTagGateSource::Flag, 5, "Olga leaves"},
    {2, DogTagGateKind::Until, DogTagGateSource::Flag, 5, "Olga leaves"},
    {3, DogTagGateKind::Until, DogTagGateSource::Flag, 5, "Olga leaves"},
    {4, DogTagGateKind::Until, DogTagGateSource::Flag, 5, "Olga leaves"},
    {5, DogTagGateKind::Until, DogTagGateSource::Flag, 6, "Tengu"},
    {6, DogTagGateKind::Until, DogTagGateSource::Flag, 6, "Tengu"},
    {7, DogTagGateKind::Until, DogTagGateSource::Flag, 6, "Tengu"},
    {8, DogTagGateKind::Until, DogTagGateSource::Flag, 6, "Tengu"},
    {9, DogTagGateKind::Until, DogTagGateSource::Flag, 6, "Tengu"},
};

constexpr const DogTagGate* dog_tag_gate(uint16_t id)
{
    for (const DogTagGate& gate : kDogTagGates) {
        if (gate.id == id) return &gate;
    }
    return nullptr;
}

constexpr bool dog_tag_gate_available(const DogTagGate& gate, uint16_t p_story, uint32_t flags)
{
    if (gate.source == DogTagGateSource::Story) {
        return gate.kind == DogTagGateKind::After ? p_story >= gate.value : p_story < gate.value;
    }
    const bool set = (flags & (uint32_t{1} << gate.value)) != 0;
    return gate.kind == DogTagGateKind::After ? set : !set;
}

// Live per-area story state shown beside the area name.
struct DogTagAreaState {
    const char* area;
    uint16_t story;
    const char* before;
    const char* after;
};

inline constexpr DogTagAreaState kDogTagAreaStates[] = {
    {"w17a", 122, "before Fatman", "after Fatman"},
    {"w19a", 122, "before Fatman", "after Fatman"},
    {"w20a", 323, "before Vamp sniper", "after Vamp sniper"},
    {"w22a", 92, "before Peter", "after Peter"},
    {"w24a", 180, "before Ames death", "after Ames death"},
    {"w24b", 176, "before Ames found", "after Ames found"},
};

static_assert(sizeof(kDogTags) / sizeof(kDogTags[0]) == 394);
static_assert(sizeof(kDogTagAreas) / sizeof(kDogTagAreas[0]) == 34);
static_assert(kDogTags[393].id == 393);

} // namespace bb::mgs2
