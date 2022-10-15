/**
 * Framework for Threes! and its variants (C++ 11)
 * agent.h: Define the behavior of variants of agents including players and environments
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <unistd.h>
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include "board.h"
#include "action.h"
#include "weight.h"

#define N 32
#define Gamma 0.99

typedef struct {
	int states[32];
} state_t;

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * base agent for agents with weight tables and a learning rate
 */
class weight_agent : public agent {
public:
	weight_agent(const std::string& args = "") : agent(args), alpha(0) {
		if (meta.find("init") != meta.end())
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end())
			load_weights(meta["load"]);
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]);
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end())
			save_weights(meta["save"]);
	}

protected:
	virtual void init_weights(const std::string& info) {
		std::string res = info; // comma-separated sizes, e.g., "65536,65536"
		for (char& ch : res)
			if (!std::isdigit(ch)) ch = ' ';
		std::stringstream in(res);
		for (size_t size; in >> size; net.emplace_back(size));
	}
	virtual void load_weights(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
	}
	virtual void save_weights(const std::string& path) {
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
	}

protected:
	std::vector<weight> net;
	float alpha;
};

/**
 * default random environment, i.e., placer
 * place the hint tile and decide a new hint tile
 */
class random_placer : public random_agent {
public:
	random_placer(const std::string& args = "") : random_agent("name=place role=placer " + args) {
		spaces[0] = { 12, 13, 14, 15 };
		spaces[1] = { 0, 4, 8, 12 };
		spaces[2] = { 0, 1, 2, 3};
		spaces[3] = { 3, 7, 11, 15 };
		spaces[4] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
	}

	virtual action take_action(const board& after) {
		std::vector<int> space = spaces[after.last()];
		std::shuffle(space.begin(), space.end(), engine);
		for (int pos : space) {
			if (after(pos) != 0) continue;

			int bag[3], num = 0;
			for (board::cell t = 1; t <= 3; t++)
				for (size_t i = 0; i < after.bag(t); i++)
					bag[num++] = t;
			std::shuffle(bag, bag + num, engine);

			board::cell tile = after.hint() ?: bag[--num];
			board::cell hint = bag[--num];

			return action::place(pos, tile, hint);
		}
		return action();
	}

private:
	std::vector<int> spaces[5];
};

/**
 * random player, i.e., slider
 * select a legal action randomly
 */
class random_slider : public weight_agent {
public:

	random_slider(const std::string& args = "") : weight_agent("name=slide role=slider " + args) {}

	void getState (state_t &state, board::grid tile, int j) {
		/*
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j)
				printf("%d ", tile[i][j]);
			printf("\n");
		}
		printf("\n");
		*/

		/*
		 *	o x x x
		 *	o x x x
		 *	o o x x
		 *	o o x x
		*/
		int tmp = 0;
		for (int i = 0; i < 4; ++i) {
			tmp += tile[i][0];
			tmp <<= 4;
		}
		tmp += tile[2][1];
		tmp <<= 4;
		tmp += tile[3][1];
		state.states[j] = tmp;
		
		/*
		 *	x o x x
		 *	x o x x
		 *	x o o x
		 *	x o o x
		*/
		tmp = 0;
		for (int i = 0; i < 4; ++i) {
			tmp += tile[i][1];
			tmp <<= 4;
		}
		tmp += tile[2][2];
		tmp <<= 4;
		tmp += tile[3][2];
		state.states[j + 1] = tmp;

		/*
		 *	x o o x
		 *	x o o x
		 *	x o o x
		 *	x x x x
		*/
		tmp = 0;
		for (int i = 0; i < 3; ++i) {
			for (int j = 1; j < 3; ++j) {
				tmp += tile[i][j];
				tmp <<= 4;
			}
		}
		state.states[j + 2] = tmp >> 4;
		
		/*
		 *	x x o o
		 *	x x o o
		 *	x x o o
		 *	x x x x
		*/
		tmp = 0;
		for (int i = 0; i < 3; ++i) {
			for (int j = 2; j < 4; ++j) {
				tmp += tile[i][j];
				tmp <<= 4;
			}
		}
		state.states[j + 3] = tmp >> 4;
	}

	void getStates (state_t &state, const board& before) {
		board s = board(before);
		for (int i = 0; i < 8; i += 2) {
			s.rotate_clockwise();
			getState(state, s.getTile(), i * 4);
			s.reflect_horizontal();
			getState(state, s.getTile(), (i + 1) * 4);
			s.reflect_horizontal();
		}
	}
	
	int choose_max_value_action (board::reward &reward, state_t &state, const board& before) {
		int maxOp = -1;
		double maxValue = -10e30;
		state_t tmpState;
		
		board::grid tile = board(before).getTile();
		for (int i = 0; i < 4; ++i) {
			board after = board(before);
			board::reward tmp = after.slide(i);
			if (tmp == -1)
				continue;
			tile = after.getTile();
			getStates(tmpState, before);
			float value = forward(tmpState); 
			if (value + tmp > maxValue) {
				for (int i = 0; i < N; ++i)
					state.states[i] = tmpState.states[i];
				maxOp = i;
				reward = tmp;
				maxValue = value + tmp;
			}
		}

		return maxOp; 
	}

	float forward (state_t state) {
		float value = 0;
		for (int i = 0; i < N; ++i)
			value += net[i].value[state.states[i]];
		return value;
	}

	virtual action take_action(const board& before) {
		board::reward reward = 0;
		state_t state;

		int maxOp = choose_max_value_action(reward, state, before);	
		
		states.push_back(state);
		rewards.push_back(reward);
		return action::slide(maxOp);
	}

	void train (board::reward next_reward, state_t next_state, state_t state) {
		for (int i = 0; i < N; ++i) {
			net[i].value[state.states[i]] += (alpha * (next_reward + Gamma * forward(next_state) - forward(state)));
		}
	}

	virtual void close_episode(const std::string& flag = "") {
		// train last afterstate
		state_t next_state = states.back();
		board::reward next_reward = rewards.back();
		for (int i = 0; i < N; ++i) {
			net[i].value[next_state.states[i]] -= (alpha * forward(next_state));
		}

		while (!states.empty()) {
			next_state = states.back();
			next_reward = rewards.back();
			states.pop_back();
			rewards.pop_back();
			if (states.empty())
				break;
			train(next_reward, next_state, states.back());
		}
	}

private:
	std::vector <state_t> states;
	std::vector <board::reward> rewards;
};
