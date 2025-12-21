#include <algorithm>
#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <set>
#include <span>
#include <cctype>
#include <optional>
#include <memory>

using namespace std;

void trim(string &s) {
	s.erase(
			s.begin(),
			find_if(s.begin(), s.end(), [](char c) { return !isspace(c); })
	);
	s.erase(
			find_if(s.rbegin(), s.rend(), [](char c) { return !isspace(c); }).base(),
			s.end()
	);
}

vector<string> list_of_strings(istream &is) {
	if(is.peek() != '[') {
		is.setstate(ios_base::failbit);
		return vector<string>();
	}
	is.get();

	string list;
	if(!getline(is, list, ']')) return vector<string>();

	istringstream ss(list);
	vector<string> strings;
	string elem;

	while(getline(ss, elem, ',')) {
		trim(elem);
		if(!elem.empty()) strings.push_back(elem);
	}

	return strings;
}

template<typename It>
void write_joined(ostream &os, It first, It last, string join = ", ") {
	if(first == last) return;
	os << *first++;
	for(; first != last; first++) os << join << *first;
}

template<typename It, typename Tr>
void write_joined(ostream &os, It first, It last, Tr transform, string join = ", ") {
	if(first == last) return;
	os << transform(*first++);
	for(; first != last; first++) os << join << transform(*first);
}

class World;

template<typename T>
concept SerNoCtx = requires(T a, istream &is, ostream &os) {
	T();
	is >> a;
	os << a;
};

template<typename T>
concept SerCtx = requires(T a, const T ac, istream &is, ostream &os, World &w, const World &wc) {
	T();
	a.read(is, w);
	ac.write(os, wc);
};

template<typename T>
concept Ser = SerNoCtx<T> || SerCtx<T>;

template<SerCtx T>
class Namespace {
	public:
		map<string, T> forward;
		map<const T *, string> inverse;

		Namespace<T> &set(const string &key, T &&value) {
			if(forward.contains(key)) {
				inverse.erase(&forward.at(key));
			}
			forward.insert_or_assign(move(key), move(value));
			inverse.insert({&forward.at(key), key});
			return *this;
		}

		void clear() {
			// In this order; clearing forward dangles inverse's pointers
			inverse.clear();
			forward.clear();
		}

		size_t size() { return forward.size(); }

		T *get(const string &key) {
			if(forward.contains(key)) return &forward.at(key);
			return nullptr;
		}

		const T* get(const string &key) const {
			if(forward.contains(key)) return &forward.at(key);
			return nullptr;
		}

		string get_name(const T *v) const {
			if(inverse.contains(v)) return inverse.at(v);
			return string();
		}

		ostream &write(ostream &os, const World &w, string indent = "  ", string end = "") const {
			os << "{" << endl;
			for(auto const &[id, elem]: forward) {
				os << indent << id << ": ";
				elem.write(os, w);
				os << endl;
			}
			os << end << "} ";
			return os;
		}

		istream &read(istream &is, World &w) {
			string temp;
			if(!(is >> temp)) return is;
			if(temp != "{") {
				is.setstate(ios_base::failbit);
				return is;
			}
			clear();
			while(true) {
				is >> ws;
				if(is.peek() == '}') {
					is.get();
					break;
				}
				getline(is, temp, ':');
				is >> ws;
				T value;
				value.read(is, w);
				set(temp, move(value));
			}
			return is;
		}
};

class Pronouns {
	public:
		string subject;
		string object;
		string possessive;
		string reflexive;
		string tense;

		enum class Part { subject, object, possessive, reflexive };

		Pronouns() : subject(), object(), possessive(), reflexive() {}
		Pronouns(const string &s, const string &o, const string &p, const string &r) :
			subject(s), object(o), possessive(p), reflexive(r) {}

		friend ostream &operator<<(ostream &os, const Pronouns &pro) {
			os << pro.subject << ' ' << pro.object << ' ' << pro.possessive
				<< ' ' << pro.reflexive << ' ' << pro.tense;
			return os;
		}

		friend istream &operator>>(istream &is, Pronouns &pro) {
			is >> pro.subject >> pro.object >> pro.possessive >> pro.reflexive >> pro.tense;
			return is;
		}

		ostream &write(ostream &os, const World &w) const {
			os << *this;
			return os;
		}

		istream &read(istream &is, World &w) {
			is >> *this;
			return is;
		}

		string get_part(Part p) const {
			switch(p) {
				case Part::subject: return subject;
				case Part::object: return object;
				case Part::possessive: return possessive;
				case Part::reflexive: return reflexive;
			}
			return string("???");
		}
};

class Player {
	public:
		string name;
		const Pronouns *pro;
		set<string> attrs;

		Player() : name(), pro(nullptr) {}
		Player(const string &name, Pronouns *pro) : name(name), pro(pro) {}

		ostream &write(ostream &os, const World &w) const;
		istream &read(istream &is, World &w);
};

class Event {
	public:
		class ActorSpec {
			public:
				set<string> matches;
				set<string> neg_matches;
				set<string> adds;
				set<string> removes;

				void clear() {
					matches.clear();
					neg_matches.clear();
					adds.clear();
					removes.clear();
				}

				bool applies_to(const Player *ply) const {
					for(const string &s: matches) {
						if(!ply->attrs.contains(s)) return false;
					}
					for(const string &s: neg_matches) {
						if(ply->attrs.contains(s)) return false;
					}
					return true;
				}

				void mutate(Player *ply) const {
					for(const string &s: removes) {
						ply->attrs.erase(s);
					}
					for(const string &s: adds) {
						ply->attrs.insert(s);
					}
				}

				friend ostream &operator<<(ostream &os, const ActorSpec &as) {
					os << "[";
					write_joined(os, as.matches.begin(), as.matches.end());
					write_joined(os, as.neg_matches.begin(), as.neg_matches.end(), [](const string &s) {
							return "!" + s;
					});
					os << "]";
					if(!as.adds.empty()) {
						os << "+[";
						write_joined(os, as.adds.begin(), as.adds.end());
						os << "]";
					}
					if(!as.removes.empty()) {
						os << "-[";
						write_joined(os, as.removes.begin(), as.removes.end());
						os << "]";
					}
					return os;
				}

				friend istream &operator>>(istream &is, ActorSpec &as) {
					vector list = list_of_strings(is);
					if(!is) return is;
					as.matches.clear();
					as.neg_matches.clear();
					for(const string &s: list) {
						if(s.at(0) == '!')
							as.neg_matches.insert(s.substr(1));
						else
							as.matches.insert(s);
					}

					as.adds.clear();
					as.removes.clear();

					is >> ws;
					if(is.peek() == '+') {
						is.get();
						list = list_of_strings(is);
						if(!is) return is;
						for(const string &s: list) as.adds.insert(s);
						is >> ws;
					}
					if(is.peek() == '-') {
						is.get();
						list = list_of_strings(is);
						if(!is) return is;
						for(const string &s: list) as.removes.insert(s);
						is >> ws;
					}
					return is;
				}

				ostream &write(ostream &os, const World &w) const {
					os << *this;
					return os;
				}

				istream &read(istream &is, World &w) {
					is >> *this;
					return is;
				}
		};

		class Renderer;

		class Binding {
			public:
				const Event &event;
				map<string, Player *> players;
				Player *last_player = nullptr;

				Binding(const Event &e, map<string, Player *> ply) : event(e), players(ply) {}
				static optional<Binding> try_bind(const Event &, const World &, vector<Player *> &, bool = true);

				friend ostream &operator<<(ostream &os, Binding &b);

				void list_refs(ostream &os) {
					os << "[";
					write_joined(os, players.begin(), players.end(), [](const auto &pair) {
							return pair.first;
					});
					os << "]";
				}

				Player *last_player_or(optional<string> name) {
					Player *ply = last_player;
					if(name) {
						if(players.contains(*name)) {
							ply = players.at(*name);
						} else {
							cerr << "bad player ref to " << *name << ": not in ";
							list_refs(cerr);
							cerr << endl;
							ply = nullptr;  // ensure usage fails
						}
					}
					return ply;
				}

				void cause_effects(World &w);
		};

		class Renderer {
			public:
				virtual ~Renderer() = default;
				virtual void render(ostream &, Binding &) = 0;
				virtual ostream &write(ostream &os, const World &w) = 0;
		};

		class render {  // XXX hacky
			public:
				class Literal : public Renderer {
					public:
						string value;
						Literal(string v) : value(v) {}

						virtual void render(ostream &os, Binding &_) {
							os << value;
						}
						virtual ostream &write(ostream &os, const World &_) {
							os << value;
							return os;
						}
				};

				class PlayerRef : public Renderer {
					public:
						string actor;
						PlayerRef(string n) : actor(n) {}

						virtual void render(ostream &os, Binding &b) {
							if(b.players.contains(actor)) {
								Player *a = b.players.at(actor);
								if(a) {
									os << a->name;
									b.last_player = a;
								}
							} else {
								cerr << "bad playerref to " << actor << ": not in ";
								b.list_refs(cerr);
								cerr << endl;
							}
						}
						virtual ostream &write(ostream &os, const World &_) {
							os << "$<" << actor << ">";
							return os;
						}
				};

				class TenseChoice : public Renderer {
					public:
						optional<string> actor;
						map<string, string> tenses;
						TenseChoice(optional<string> a, map<string, string> t) : actor(a), tenses(t) {}

						virtual void render(ostream &os, Binding &b) {
							Player *ply = b.last_player_or(actor);
							if(!ply) {
								cerr << "tensechoice has no actor--either it was used before any playerref or no player was bound to the named ref" << endl;
								return;
							}
							if(!ply->pro) {
								cerr << "player " << ply->name << " has no pronouns, can't pick a tense " << endl;
								return;
							}
							string t = ply->pro->tense;
							if(tenses.contains(t))
								os << tenses.at(t);
						}
						virtual ostream &write(ostream &os, const World &_) {
							os << "[";
							if(actor) {
								os << "(" << *actor << ")";
							}
							write_joined(os, tenses.begin(), tenses.end(), [](const auto &pair) {
									const auto &[tense, repl] = pair;
									return tense + "=" + repl;
							});
							os << "]";
							return os;
						}
				};

				class Pronoun : public Renderer {
					public:
						optional<string> actor;
						Pronouns::Part part;
						bool upcase;
						Pronoun(optional<string> a, Pronouns::Part p, bool u) : actor(a), part(p), upcase(u) {}

						virtual void render(ostream &os, Binding &b) {
							Player *ply = b.last_player_or(actor);
							if(!ply) {
								cerr << "pronoun has no actor--either it was used before any playerref or no player was bound to the named ref" << endl;
								return;
							}
							if(!ply->pro) {
								cerr << "player " << ply->name << " has no pronouns, can't use one" << endl;
								return;
							}
							string pro = ply->pro->get_part(part);
							if(!pro.empty() && upcase) {
								pro[0] = toupper(pro[0]);
							}
							os << pro;
							b.last_player = ply;
						}
						virtual ostream &write(ostream &os, const World &_) {
							os << "<";
							if(actor) {
								os << "(" << *actor << ")";
							}
							string p;
							switch(part) {
								case Pronouns::Part::subject: p = "s"; break;
								case Pronouns::Part::object: p = "o"; break;
								case Pronouns::Part::possessive: p = "p"; break;
								case Pronouns::Part::reflexive: p = "r"; break;
							}
							if(upcase) {
								p[0] = toupper(p[0]);
							}
							os << p << ">";
							return os;
						}
				};

				class PossessiveParticle : public Renderer {
					public:
						optional<string> actor;
						PossessiveParticle(optional<string> a) : actor(a) {}

						virtual void render(ostream &os, Binding &b) {
							Player *ply = b.last_player_or(actor);
							if(!ply) {
								cerr << "possessiveparticle has no actor--either it was used before any playerref or no player was bound to the named red" << endl;
								return;
							}
							if(!ply->name.empty() && tolower(ply->name.back()) == 's') {
								os << "'";
							} else {
								os << "'s";
							}
						}
						virtual ostream &write(ostream &os, const World &_) {
							os << "<";
							if(actor) {
								os << "(" << *actor << ")";
							}
							os << "'s>";
							return os;
						}
				};

				static optional<string> parse_maybe_paren_name(istringstream &ss) {
					if(ss.peek() == '(') {
						ss.get();
						string name;
						getline(ss, name, ')');
						return make_optional(name);
					}
					return optional<string>();
				}

				static vector<unique_ptr<Renderer>> parse_message(string msg) {
					istringstream ss(msg);
					string literal;
					vector<unique_ptr<Renderer>> result;

					while(!ss.eof()) {
						char c = ss.get();
						switch(c) {
							case '$': {
								if(!literal.empty()) {
									result.push_back(
											unique_ptr<Renderer>(
												new Literal(literal)
											)
									);
									literal.clear();
								}
								string ref;
								if(ss.peek() == '<') {
									ss.get();
									getline(ss, ref, '>');
								} else {
									ss >> ref;
								}
								result.push_back(
										unique_ptr<Renderer>(
											new PlayerRef(ref)
										)
								);
								break;
							}

							case '[': {
								if(!literal.empty()) {
									result.push_back(
											unique_ptr<Renderer>(
												new Literal(literal)
											)
									);
									literal.clear();
								}
								optional<string> actor = parse_maybe_paren_name(ss);
								string contents;
								getline(ss, contents, ']');
								istringstream reader(contents);
								map<string, string> tenses;
								string elem;
								while(getline(reader, elem, '/')) {
									trim(elem);
									if(elem.empty()) continue;
									auto pos = elem.find('=');
									if(pos == string::npos) continue;
									string k = elem.substr(0, pos);
									string v = elem.substr(pos + 1);
									tenses.insert_or_assign(k, v);
								}
								result.push_back(
										unique_ptr<Renderer>(
											new TenseChoice(actor, tenses)
										)
								);
								break;
							}

							case '<': {
								if(!literal.empty()) {
									result.push_back(
											unique_ptr<Renderer>(
												new Literal(literal)
											)
									);
									literal.clear();
								}
								optional<string> actor = parse_maybe_paren_name(ss);
								Pronouns::Part p;
								string contents;
								getline(ss, contents, '>');
								bool upcase = false;
								if(!contents.empty() && isupper(contents[0])) {
									contents[0] = tolower(contents[0]);
									upcase = true;
								}
								if(contents == "s") {
									p = Pronouns::Part::subject;
								} else if(contents == "o") {
									p = Pronouns::Part::object;
								} else if(contents == "p") {
									p = Pronouns::Part::possessive;
								} else if(contents == "r") {
									p = Pronouns::Part::reflexive;
								} else if(contents == "'s") {
									result.push_back(
											unique_ptr<Renderer>(
												new PossessiveParticle(actor)
											)
									);
									break;
								} else {
									cerr << "unknown pronoun spec " << contents << "--I only know s, o, p, and r (and their uppercase variants)" << endl;
									break;
								}
								result.push_back(
										unique_ptr<Renderer>(
											new Pronoun(actor, p, upcase)
										)
								);
								break;
							}

							default: {
								literal.push_back(c);
								break;
							}
						}
					}

					if(!literal.empty()) {
						result.push_back(
								unique_ptr<Renderer>(
									new Literal(literal)
								)
						);
					}

					return result;
				}

		};

		Namespace<ActorSpec> actors;
		ActorSpec world_spec;
		vector<unique_ptr<Renderer>> render;

		size_t involved_actors() { return actors.size(); }

		ostream &write(ostream &os, const World &w) const;
		istream &read(istream &is, World &w);
};

ostream& operator<<(ostream &os, Event::Binding &b) {
	for(const unique_ptr<Event::Renderer> &r: b.event.render)
		r->render(os, b);
	return os;
}

class World {
	public:
		Namespace<Pronouns> pronouns;
		Namespace<Player> players;
		Namespace<Event> events;
		Player world_player{"<world>", nullptr};

	friend ostream &operator<<(ostream &os, const World &w) {
		os << "pronouns ";
		w.pronouns.write(os, w);
		os << endl;
		os << "players ";
		w.players.write(os, w);
		os << endl;
		os << "world [";
		write_joined(os, w.world_player.attrs.begin(), w.world_player.attrs.end());
		os << "]" << endl;
		os << "events ";
		w.events.write(os, w);
		os << endl;
		return os;
	}

	friend istream &operator>>(istream &is, World &w) {
		w.pronouns.clear();
		w.players.clear();
		w.events.clear();
		w.world_player.attrs.clear();

		string section;
		while(is >> section) {
			if(section == "pronouns") {
				w.pronouns.read(is, w);
				is >> ws;
			} else if(section == "players") {
				w.players.read(is, w);
				is >> ws;
			} else if(section == "events") {
				w.events.read(is, w);
				is >> ws;
			} else if(section == "world") {
				is >> ws;
				vector<string> attrs = list_of_strings(is);
				for(const string &s: attrs)
					w.world_player.attrs.insert(s);
			} else {
				cerr << "non-section: " << section << endl;
				break;
			}
		}

		return is;
	}
};

optional<Event::Binding> Event::Binding::try_bind(const Event &e, const World &w, vector<Player *> &players, bool use_attrs) {
	if(use_attrs && !e.world_spec.applies_to(&w.world_player)) return optional<Binding>();

	map<string, Player *> bindings;

	for(const auto &[name, spec]: e.actors.forward) {
		auto it = players.begin();
		bool found = false;
		while(it != players.end()) {
			if(!use_attrs || spec.applies_to(*it)) {
				bindings.insert_or_assign(name, *it);
				players.erase(it);
				found = true;
				break;
			}
			it++;
		}
		if(!found) return optional<Binding>();
	}

	return make_optional(Binding(e, bindings));
}

void Event::Binding::cause_effects(World &w) {
	for(auto &[key, ply]: players) {
		auto spec = event.actors.get(key);
		if(spec) {
			spec->mutate(ply);
		}
	}
	event.world_spec.mutate(&w.world_player);
}

class Round {
	public:
		World &world;
		mt19937 rng;
		vector<Player *> player_pool;
		vector<Event *> player_events;
		vector<Event *> unassoc_events;
		vector<Event::Binding> bindings;

		int unassoc_chance = 1;

		Round(World &w, mt19937 rng) : world(w), rng(rng) {
			player_pool.reserve(world.players.size());
			for(auto &[_, player]: world.players.forward) {
				Player *ply = &player;
				player_pool.push_back(ply);
			}

			for(auto &[_, event]: world.events.forward) {
				Event *ev = &event;
				if(ev->involved_actors() > 0)
					player_events.push_back(ev);
				else
					unassoc_events.push_back(ev);
			}

			shuffle(player_pool.begin(), player_pool.end(), rng);
			shuffle(player_events.begin(), player_events.end(), rng);
			shuffle(unassoc_events.begin(), unassoc_events.end(), rng);
		}

		void cause_player_event() {
			if(player_pool.empty() || player_events.empty()) return;

			vector<Player *> pool(player_pool);
			while(!player_events.empty()) {
				Event *ev = player_events.back();
				player_events.pop_back();
				auto b = Event::Binding::try_bind(*ev, world, pool);
				if(b) {
					bindings.push_back(*b);
					player_pool = pool;
					return;
				}
			}
		}

		void cause_unassoc_event() {
			if(unassoc_events.empty()) return;

			vector<Player *> no_pool;
			while(!unassoc_events.empty()) {
				Event *ev = unassoc_events.back();
				unassoc_events.pop_back();
				auto b = Event::Binding::try_bind(*ev, world, no_pool);
				if(b) {
					bindings.push_back(*b);
				}
			}
		}

		void resolve() {
			while(!player_pool.empty() && !player_events.empty()) {
				cause_player_event();
			}
			while(!unassoc_events.empty()) {
				if(rng() % unassoc_chance != 0) break;
				cause_unassoc_event();
			}
			for(auto &b: bindings) {
				b.cause_effects(world);
			}
		}

		friend ostream &operator<<(ostream &os, Round &r) {
			for(auto &b: r.bindings) {
				os << b << endl;
			}
			return os;
		}
};

ostream &Player::write(ostream &os, const World &w) const {
	os << name << "(";
	string p = w.pronouns.get_name(pro);
	os << p << ")[";
	write_joined(os, attrs.begin(), attrs.end());
	os << "]";
	return os;
}

istream &Player::read(istream &is, World &w) {
	if(!getline(is, name, '(')) return is;
	string pkey;
	if(!getline(is, pkey, ')')) return is;
	pro = w.pronouns.get(pkey);

	attrs.clear();
	is >> ws;
	if(is.peek() == '[') {
		vector list = list_of_strings(is);
		if(!is) return is;
		for(const string &attr: list) attrs.insert(attr);
	}

	return is;
}

ostream &Event::write(ostream &os, const World &w) const {
	os << "{ needs ";
	actors.write(os, w, "    ", "  ");
	os << " world " << world_spec << " message {";
	for(const auto &r: render)
		r->write(os, w);
	os << "} }" << endl;
	return os;
}

istream &Event::read(istream &is, World &w) {
	if(is.peek() != '{') {
		is.setstate(ios_base::failbit);
		return is;
	}
	is.get();

	actors.clear();
	world_spec.clear();
	render.clear();

	string section;
	while(true) {
		is >> ws;
		if(is.peek() == '}') {
			is.get();
			is >> ws;
			break;
		}

		if(!(is >> section)) break;
		if(section == "needs") {
			actors.read(is, w);
		} else if(section == "world") {
			is >> ws;
			is >> world_spec;
		} else if(section == "message") {
			is >> ws;
			if(is.peek() != '{') {
				is.setstate(ios_base::failbit);
				return is;
			}
			is.get();
			string message;
			getline(is, message, '}');
			render = Event::render::parse_message(message);
		} else break;
	}

	return is;
}

void usage() {
	cerr << "I know the following arguments (no options yet!):" << endl;
	cerr << " - cat -- just output the world that was input. useful for testing and validation" << endl;
	cerr << " - try_events -- try every event in the set (to be sure they print), as long as enough players exist" << endl;
	cerr << " - round -- run a round of simulation generating logs" << endl;
}

int main(int argc, char **argv) {
	vector<string> args(argc);

	for(int i = 0; i < argc; i++) {
		args[i] = string(argv[i]);
	}

	if(args.size() < 2) {
		usage();
		return 1;
	}

	string action = args.at(1);

	World w;
	cin >> w;

	if(action == "cat") {
		cout << w;
	} else if(action == "try_events") {
		vector<Player *> players;
		players.reserve(w.players.size());
		for(auto &[_, ply]: w.players.forward) players.push_back(&ply);

		for(const auto &[evname, event]: w.events.forward) {
			vector<Player *> list(players);
			optional<Event::Binding> b = Event::Binding::try_bind(event, w, list, false);
			if(!b) {
				cerr << "Failed to bind for event " << evname << "; maybe there aren't enough players?" << endl;
			} else {
				cout << *b << endl;
			}
		}
	} else if(action == "round") {
		random_device rd;
		Round r(w, mt19937(rd()));
		r.resolve();
		cout << w << endl;
		cout << "---" << endl;
		cout << r << endl;
	}

	return 0;
}
