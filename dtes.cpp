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
requires requires(It it, Tr f) {
	{ f(*it) } -> convertible_to<string>;
}
void write_joined(ostream &os, It first, It last, Tr transform, string join = ", ") {
	if(first == last) return;
	os << transform(*first++);
	for(; first != last; first++) os << join << transform(*first);
}

string colon_sep(const pair<string, string> &pair) {
	return pair.first + ":" + pair.second;
}

string colon_sep_triple(const tuple<string, string, string> &tup) {
	const auto &[one, two, three] = tup;
	return one + ":" + two + ":" + three;
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
		map<string, string> props;

		Player() : name(), pro(nullptr) {}
		Player(const string &name, Pronouns *pro) : name(name), pro(pro) {}

		ostream &write(ostream &os, const World &w) const;
		istream &read(istream &is, World &w);
};

class Relation {
	public:
		bool directional;
		bool allow_reflex;
		set<pair<Player *, Player *>> edges;

		void insert(Player *left, Player *right) {
			if(!allow_reflex && left == right) return;
			edges.insert({left, right});
			if(!directional)
				edges.insert({right, left});
		}

		void erase(Player *left, Player *right) {
			edges.erase({left, right});
			if(!directional)
				edges.erase({right, left});
		}

		bool contains(Player *left, Player *right) const {
			return edges.contains({left, right});
		}

		ostream &write(ostream &os, const World &w) const;
		istream &read(istream &is, World &w);
};

class Event {
	public:
		class Binding;

		class ActorSpec {
			public:
				set<string> attr_matches;
				set<string> attr_neg_matches;
				set<string> attr_adds;
				set<string> attr_removes;

				map<string, string> prop_matches;
				map<string, string> prop_neg_matches;
				map<string, string> prop_adds;
				map<string, string> prop_removes;

				void clear() {
					attr_matches.clear();
					attr_neg_matches.clear();
					attr_adds.clear();
					attr_removes.clear();

					prop_matches.clear();
					prop_neg_matches.clear();
					prop_adds.clear();
					prop_removes.clear();
				}

				bool applies_to(const Player *ply) const {
					for(const string &s: attr_matches) {
						if(!ply->attrs.contains(s)) return false;
					}
					for(const string &s: attr_neg_matches) {
						if(ply->attrs.contains(s)) return false;
					}
					for(const auto &[key, val]: prop_matches) {
						if(val.empty()) {
							if(!ply->props.contains(key)) return false;
						} else {
							if(!ply->props.contains(key) || ply->props.at(key) != val) return false;
						}
					}
					for(const auto &[key, val]: prop_neg_matches) {
						if(val.empty()) {
							if(ply->props.contains(key)) return false;
						} else {
							if(ply->props.contains(key) && ply->props.at(key) == val) return false;
						}
					}
					return true;
				}

				void mutate_additions(Player *ply, Binding &b) const; 
				void mutate_deletions(Player *ply, Binding &b) const; 

				friend ostream &operator<<(ostream &os, const ActorSpec &as) {
					os << "[";
					vector<string> specs;
					auto append_spec = back_inserter(specs);
					copy(as.attr_matches.begin(), as.attr_matches.end(), append_spec);
					transform(as.attr_neg_matches.begin(), as.attr_neg_matches.end(), append_spec, [](const string &s) {
							return "!" + s;
					});
					transform(as.prop_matches.begin(), as.prop_matches.end(), append_spec, colon_sep);
					transform(as.prop_neg_matches.begin(), as.prop_neg_matches.end(), append_spec, [](const auto &pair) {
							return "!" + colon_sep(pair);
					});
					write_joined(os, specs.begin(), specs.end());
					os << "]";
					if(!(as.attr_adds.empty() && as.prop_adds.empty())) {
						os << "+[";
						specs.clear();
						copy(as.attr_adds.begin(), as.attr_adds.end(), append_spec);
						transform(as.prop_adds.begin(), as.prop_adds.end(), append_spec, colon_sep);
						write_joined(os, specs.begin(), specs.end());
						os << "]";
					}
					if(!(as.attr_removes.empty() && as.prop_removes.empty())) {
						os << "-[";
						specs.clear();
						copy(as.attr_removes.begin(), as.attr_removes.end(), append_spec);
						transform(as.prop_removes.begin(), as.prop_removes.end(), append_spec, colon_sep);
						write_joined(os, specs.begin(), specs.end());
						os << "]";
					}
					return os;
				}

				friend istream &operator>>(istream &is, ActorSpec &as) {
					vector list = list_of_strings(is);
					if(!is) return is;
					as.attr_matches.clear();
					as.attr_neg_matches.clear();
					for(const string &s: list) {
						if(s.at(0) == '!') {
							auto colon = s.find(':');
							if(colon != string::npos) {
								as.prop_neg_matches.insert_or_assign(s.substr(1, colon - 1), s.substr(colon + 1));
							} else {
								as.attr_neg_matches.insert(s.substr(1));
							}
						} else {
							auto colon = s.find(':');
							if(colon != string::npos) {
								as.prop_matches.insert_or_assign(s.substr(0, colon), s.substr(colon + 1));
							} else {
								as.attr_matches.insert(s);
							}
						}
					}

					as.attr_adds.clear();
					as.attr_removes.clear();

					is >> ws;
					if(is.peek() == '+') {
						is.get();
						list = list_of_strings(is);
						if(!is) return is;
						for(const string &s: list) {
							auto colon = s.find(':');
							if(colon != string::npos) {
								as.prop_adds.insert_or_assign(s.substr(0, colon), s.substr(colon + 1));
							} else {
								as.attr_adds.insert(s);
							}
						}
						is >> ws;
					}
					if(is.peek() == '-') {
						is.get();
						list = list_of_strings(is);
						if(!is) return is;
						for(const string &s: list) {
							auto colon = s.find(':');
							if(colon != string::npos) {
								as.prop_removes.insert_or_assign(s.substr(0, colon), s.substr(colon + 1));
							} else {
								as.attr_removes.insert(s);
							}
						}
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

		class RelSpec {
			public:
				using Triple = tuple<string, string, string>;
				set<Triple> matches;
				set<Triple> neg_matches;
				set<Triple> adds;
				set<Triple> removes;

				void clear() {
					matches.clear();
					neg_matches.clear();
					adds.clear();
					removes.clear();
				}

				bool empty() const {
					return matches.empty() && neg_matches.empty();
				}

				bool satisfied(Binding &b, const World &w) const;
				void mutate(Binding &b, World &w) const;

				friend ostream &operator<<(ostream &os, const RelSpec &rs) {
					os << "{ ";
					vector<string> specs;
					auto append_spec = back_inserter(specs);
					transform(rs.matches.begin(), rs.matches.end(), append_spec, colon_sep_triple);
					transform(rs.neg_matches.begin(), rs.neg_matches.end(), append_spec, [](const Triple &t) {
							return "!" + colon_sep_triple(t);
					});
					transform(rs.adds.begin(), rs.adds.end(), append_spec, [](const Triple &t) {
							return "+" + colon_sep_triple(t);
					});
					transform(rs.removes.begin(), rs.removes.end(), append_spec, [](const Triple &t) {
							return "-" + colon_sep_triple(t);
					});
					write_joined(os, specs.begin(), specs.end(), " ");
					os << " }";
					return os;
				}

				friend istream &operator>>(istream &is, RelSpec &rs) {
					is >> ws;
					if(is.peek() != '{') {
						is.setstate(ios_base::failbit);
						return is;
					}
					is.get();

					rs.clear();
					string elem;
					while(is >> ws, is >> elem, elem != "}") {
						if(elem.empty()) continue;
						set<Triple> *mod = &rs.matches;
						if(elem.at(0) == '!') {
							mod = &rs.neg_matches;
							elem = elem.substr(1);
						} else if(elem.at(0) == '+') {
							mod = &rs.adds;
							elem = elem.substr(1);
						} else if(elem.at(0) == '-') {
							mod = &rs.removes;
							elem = elem.substr(1);
						}

						istringstream parser(elem);
						string left, rel, right;
						if(!getline(parser, left, ':')) continue;
						if(!getline(parser, rel, ':')) continue;
						right.append(istreambuf_iterator<char>(parser), istreambuf_iterator<char>());
						if(left.empty() || rel.empty() || right.empty()) continue;
						mod->insert(Triple(left, rel, right));
					}

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

				class PropRef : public Renderer {
					public:
						optional<string> actor;
						string prop;
						PropRef(optional<string> a, string p): actor(a), prop(p) {}

						virtual void render(ostream &os, Binding &b) {
							Player *ply = b.last_player_or(actor);
							if(!ply) {
								cerr << "propref has no actor--either it was used before any playerref or no player was bound to the named ref" << endl;
								return;
							}
							b.last_player = ply;
							if(ply->props.contains(prop)) {
								os << ply->props.at(prop);
							}
						}
						virtual ostream &write(ostream &os, const World &_) {
							os << "$<";
							if(actor) {
								os << *actor;
							}
							os << "." << prop << ">";
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

				static vector<unique_ptr<Renderer>> parse_message(const string &msg) {
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
								auto dot = ref.find('.');
								if(dot != string::npos) {
									const string actorn = ref.substr(0, dot);
									const string prop = ref.substr(dot + 1);
									optional<string> actor;
									if(!actorn.empty()) actor = actorn;
									result.push_back(
											unique_ptr<Renderer>(
												new PropRef(actor, prop)
											)
									);
								} else {
									result.push_back(
											unique_ptr<Renderer>(
												new PlayerRef(ref)
											)
									);
								}
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
		RelSpec rel;
		vector<unique_ptr<Renderer>> render;
		int multiplicity = 1, unlikeliness = 1;

		size_t involved_actors() { return actors.size(); }

		template<typename RNG>
		bool should_happen(RNG &rng) { return rng() % unlikeliness == 0; }

		ostream &write(ostream &os, const World &w) const;
		istream &read(istream &is, World &w);
};

ostream& operator<<(ostream &os, Event::Binding &b) {
	for(const unique_ptr<Event::Renderer> &r: b.event.render)
		r->render(os, b);
	return os;
}

void Event::ActorSpec::mutate_additions(Player *ply, Event::Binding &b) const {
	for(const string &s: attr_adds) {
		ply->attrs.insert(s);
	}
	for(const auto &[key, val]: prop_adds) {
		if(val.empty()) {
			ply->props.erase(key);
		} else {
			auto msg = Event::render::parse_message(val);
			ostringstream out;
			for(const auto &cmp: msg) cmp->render(out, b);
			ply->props.insert_or_assign(key, out.str());
		}
	}
}

void Event::ActorSpec::mutate_deletions(Player *ply, Event::Binding &b) const {
	for(const string &s: attr_removes) {
		ply->attrs.erase(s);
	}
	for(const auto &[key, val]: prop_removes) {
		if(val.empty()) {
			ply->props.erase(key);
		} else {
			auto msg = Event::render::parse_message(val);
			ostringstream out;
			for(const auto &cmp: msg) cmp->render(out, b);
			if(ply->props.contains(key) && ply->props.at(key) == out.str())
				ply->props.erase(key);
		}
	}
}

class World {
	public:
		Namespace<Pronouns> pronouns;
		Namespace<Player> players;
		Namespace<Event> events;
		Namespace<Relation> relations;
		Player world_player{"<world>", nullptr};

	friend ostream &operator<<(ostream &os, const World &w) {
		os << "pronouns ";
		w.pronouns.write(os, w);
		os << endl;
		os << "players ";
		w.players.write(os, w);
		os << endl;
		os << "relations ";
		w.relations.write(os, w);
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
			} else if(section == "relations") {
				w.relations.read(is, w);
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

bool Event::RelSpec::satisfied(Event::Binding &b, const World &w) const {
	for(const auto &[left, rel, right]: matches) {
		const Relation *rp = w.relations.get(rel);
		if(!rp) {
			cerr << "relspec: relation " << rel << " does not exist" << endl;
			continue;
		}
		Player *l = b.players[left], *r = b.players[right];
		if(!l) {
			cerr << "relspec: needsref " << left << " does not exist" << endl;
			continue;
		}
		if(!r) {
			cerr << "relspec: needsref " << right << " does not exist" << endl;
			continue;
		}
		if(!rp->contains(l, r)) return false;
	}
	for(const auto &[left, rel, right]: neg_matches) {
		const Relation *rp = w.relations.get(rel);
		if(!rp) {
			cerr << "relspec: relation " << rel << " does not exist" << endl;
			continue;
		}
		Player *l = b.players[left], *r = b.players[right];
		if(!l) {
			cerr << "relspec: needsref " << left << " does not exist" << endl;
			continue;
		}
		if(!r) {
			cerr << "relspec: needsref " << right << " does not exist" << endl;
			continue;
		}
		if(rp->contains(l, r)) return false;
	}
	// a bit of a special case of separating the matcher/mutator duty: don't allow
	// an add to execute that would violate a reflex
	for(const auto &[left, rel, right]: adds) {
		const Relation *rp = w.relations.get(rel);
		if(!rp) {
			cerr << "relspec: relation " << rel << " does not exist" << endl;
			continue;
		}
		Player *l = b.players[left], *r = b.players[right];
		if(!l) {
			cerr << "relspec: needsref " << left << " does not exist" << endl;
			continue;
		}
		if(!r) {
			cerr << "relspec: needsref " << right << " does not exist" << endl;
			continue;
		}
		if(!rp->allow_reflex && l == r) return false;
	}
	return true;
}

void Event::RelSpec::mutate(Event::Binding &b, World &w) const {
	for(const auto &[left, rel, right]: adds) {
		Relation *rp = w.relations.get(rel);
		if(!rp) {
			cerr << "relspec: relation " << rel << " does not exist" << endl;
			continue;
		}
		Player *l = b.players[left], *r = b.players[right];
		if(!l) {
			cerr << "relspec: needsref " << left << " does not exist" << endl;
			continue;
		}
		if(!r) {
			cerr << "relspec: needsref " << right << " does not exist" << endl;
			continue;
		}
		rp->insert(l, r);
	}
	for(const auto &[left, rel, right]: removes) {
		Relation *rp = w.relations.get(rel);
		if(!rp) {
			cerr << "relspec: relation " << rel << " does not exist" << endl;
			continue;
		}
		Player *l = b.players[left], *r = b.players[right];
		if(!l) {
			cerr << "relspec: needsref " << left << " does not exist" << endl;
			continue;
		}
		if(!r) {
			cerr << "relspec: needsref " << right << " does not exist" << endl;
			continue;
		}
		rp->erase(l, r);
	}
}

static optional<Event::Binding> _try_bind_fastpath(const Event &e, const World &w, vector<Player *> &players, bool use_attrs) {
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
		if(!found) return optional<Event::Binding>();
	}

	return make_optional(Event::Binding(e, bindings));
}

optional<Event::Binding> Event::Binding::try_bind(const Event &e, const World &w, vector<Player *> &players, bool use_attrs) {
	if(use_attrs && !e.world_spec.applies_to(&w.world_player)) return optional<Binding>();
	if(!use_attrs || e.rel.empty()) return _try_bind_fastpath(e, w, players, use_attrs);

	// theorem: use_attrs is asserted here
	map<string, vector<Player *>> candidates;
	for(const auto &[name, spec]: e.actors.forward) {
		vector<Player *> avail;
		for(Player *p: players) {
			if(spec.applies_to(p))
				avail.push_back(p);
		}
		if(avail.empty()) return optional<Event::Binding>();  // no way to proceed if any set is empty
		candidates.insert_or_assign(name, avail);
	}

	// we need an order on keys, so we assemble it here
	vector<string> keys;
	vector<vector<Player *>> sets;
	vector<int> indices;
	for(const auto &[name, set]: candidates) {
		keys.push_back(name);
		indices.push_back(0);
		sets.push_back(set);
	}

	while(true) {
		map<string, Player *> bindings;
		set<Player *> seen;
		bool skip = false;
		for(int i = 0; i < keys.size(); i++) {
			Player *p = sets[i][indices[i]];
			if(seen.contains(p)) {
				skip = true;
				break;
			}
			seen.insert(p);
			bindings.insert_or_assign(keys[i], p);
		}
		if(!skip) {
			Binding b(e, bindings);
			if(e.rel.satisfied(b, w)) return make_optional(b);
		}

		bool exhausted = true;
		for(int i = 0; i < keys.size(); i++) {
			indices[i]++;
			if(indices[i] >= sets[i].size()) {
				indices[i] = 0;
			} else {
				exhausted = false;
				break;
			}
		}
		if(exhausted) break;
	}
	return optional<Event::Binding>();
}

void Event::Binding::cause_effects(World &w) {
	// We make this two-pass here because props can depend on (the rendering of)
	// other props, and thus it's a bad idea to remove them first when they may
	// be referenced elsewhere.

	for(auto &[key, ply]: players) {
		auto spec = event.actors.get(key);
		if(spec) {
			spec->mutate_additions(ply, *this);
		}
	}
	event.world_spec.mutate_additions(&w.world_player, *this);

	for(auto &[key, ply]: players) {
		auto spec = event.actors.get(key);
		if(spec) {
			spec->mutate_deletions(ply, *this);
		}
	}
	event.world_spec.mutate_deletions(&w.world_player, *this);

	event.rel.mutate(*this, w);
}

class Round {
	public:
		World &world;
		mt19937 rng;
		vector<Player *> player_pool;
		vector<Event *> player_events;
		vector<Event *> unassoc_events;
		vector<Event::Binding> bindings;

		vector<string> messages;

		Round(World &w, mt19937 rng) : world(w), rng(rng) {
			player_pool.reserve(world.players.size());
			for(auto &[_, player]: world.players.forward) {
				Player *ply = &player;
				player_pool.push_back(ply);
			}

			for(auto &[_, event]: world.events.forward) {
				Event *ev = &event;
				for(int i = 0; i < ev->multiplicity; i++)
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
				if(!ev->should_happen(rng)) return;
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
				if(!ev->should_happen(rng)) return;
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
				cause_unassoc_event();
			}
			for(auto &b: bindings) {
				ostringstream os;
				os << b;
				string message = os.str();
				if(!message.empty())
					messages.push_back(message);
			}
			for(auto &b: bindings) {
				b.cause_effects(world);
			}
		}

		friend ostream &operator<<(ostream &os, Round &r) {
			for(auto &s: r.messages) {
				os << s << endl;
			}
			return os;
		}
};

ostream &Player::write(ostream &os, const World &w) const {
	os << name << "(";
	string p = w.pronouns.get_name(pro);
	os << p << ")[";
	vector<string> specs;
	auto append_spec = back_inserter(specs);
	copy(attrs.begin(), attrs.end(), append_spec);
	transform(props.begin(), props.end(), append_spec, colon_sep);
	write_joined(os, specs.begin(), specs.end());
	os << "]";
	return os;
}

istream &Player::read(istream &is, World &w) {
	if(!getline(is, name, '(')) return is;
	string pkey;
	if(!getline(is, pkey, ')')) return is;
	pro = w.pronouns.get(pkey);

	attrs.clear();
	props.clear();
	is >> ws;
	if(is.peek() == '[') {
		vector list = list_of_strings(is);
		if(!is) return is;
		for(const string &attr: list) {
			auto colon = attr.find(':');
			if(colon != string::npos) {
				const string name = attr.substr(0, colon);
				const string value = attr.substr(colon + 1);
				if(!(name.empty() || value.empty())) {
					props.insert_or_assign(name, value);
				}
			} else {
				attrs.insert(attr);
			}
		}
	}

	return is;
}

ostream &Relation::write(ostream &os, const World &w) const {
	if(directional) {
		os << "dir";
	} else {
		os << "undir";
	}
	if(allow_reflex) {
		os << " reflex";
	}
	os << " {" << endl;
	for(const auto &[lp, rp]: edges) {
		const string lname = w.players.get_name(lp), rname = w.players.get_name(rp);
		if(!(lname.empty() || rname.empty())) {
			os << "    " << lname << " " << rname << endl;
		}
	}
	os << "  }";
	return os;
}

istream &Relation::read(istream &is, World &w) {
	string dir;
	if(!(is >> dir)) return is;
	if(dir == "dir")
		directional = true;
	else if(dir == "undir")
		directional = false;
	else {
		is.setstate(ios_base::failbit);
		return is;
	}
	allow_reflex = false;
	is >> ws;
	if(is.peek() != '{') {
		string token;
		if(!(is >> token)) return is;
		if(token == "reflex") {
			allow_reflex = true;
		} else {
			is.setstate(ios_base::failbit);
			return is;
		}
		is >> ws;
		if(is.peek() != '{') {
			is.setstate(ios_base::failbit);
			return is;
		}
	}
	is.get();

	while(true) {
		string left, right;
		is >> left;
		if(left == "}") break;
		is >> right;
		Player *lp = w.players.get(left), *rp = w.players.get(right);
		if(!lp) {
			cerr << "bad player name " << left << " in relation" << endl;
		}
		if(!rp) {
			cerr << "bad player name " << right << " in relation" << endl;
		}
		if(!lp || !rp) continue;
		insert(lp, rp);
	}
	return is;
}

ostream &Event::write(ostream &os, const World &w) const {
	os << "{ needs ";
	actors.write(os, w, "    ", "  ");
	os << " world " << world_spec << " rel " << rel << " chance " << multiplicity << "/" << unlikeliness << " message {";
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
		} else if(section == "chance") {
			is >> multiplicity;
			is >> ws;
			if(is.peek() == '/') {
				is.get();
				is >> unlikeliness;
				is >> ws;
			}
		} else if(section == "rel") {
			is >> ws;
			is >> rel;
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
	cerr << " - try_event <event> <needid>:<playerid>... -- print out an event with manually-specified bindings" << endl;
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
	} else if(action == "try_event") {
		if(args.size() < 3) {
			cerr << "try_event <event> <needid>:<playerid>..." << endl;
			return 1;
		}
		string evid = args.at(2);
		if(!w.events.forward.contains(evid)) {
			cerr << "no event named " << evid << "; the events are [";
			write_joined(cerr, w.events.forward.begin(), w.events.forward.end(), [](const auto &pair) {
					return pair.first;
			});
			cerr << "]" << endl;
			return 1;
		}
		Event &ev = w.events.forward.at(evid);
		if(args.size() - 3 != ev.involved_actors()) {
			cerr << "event expects " << ev.involved_actors() << " actors; you supplied " << (args.size() - 3) << endl;
			return 1;
		}
		map<string, Player *> bindings;
		for(auto it = args.begin() + 3; it != args.end(); it++) {
			auto pos = it->find(':');
			if(pos == string::npos) {
				cerr << "needid:playerid spec " << *it << " is invalid--need a colon" << endl;
				return 1;
			}
			string needid = it->substr(0, pos);
			string playerid = it->substr(pos + 1);
			if(!ev.actors.forward.contains(needid)) {
				cerr << "event does not contain a needid " << needid << endl;
				return 1;
			}
			if(!w.players.forward.contains(playerid)) {
				cerr << "no such playerid " << playerid << endl;
				return 1;
			}
			bindings.insert_or_assign(needid, &w.players.forward.at(playerid));
		}
		auto binding = Event::Binding(ev, bindings);
		binding.cause_effects(w);
		cout << w << "---" << endl << binding << endl;
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
