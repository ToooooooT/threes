/**
 * Framework for Threes! and its variants (C++ 11)
 * agent.h: Define the behavior of variants of agents including players and environments
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <climits>
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
class random_slider : public random_agent {
public:
	unsigned int count_move = 0;

	random_slider(const std::string& args = "") : random_agent("name=slide role=slider " + args),
		opcode({ 0, 1, 2, 3 }) {}

	void add_reward_by_hint (board::grid tile, board::reward reward_of_op[4], unsigned int hint_tile, int op, unsigned int count_move) {
		int factor, secFactor = 1;
		if (count_move > 200)
			factor = 4;
		else
			factor = 3;
		if (op == 2) {
			for (int j = 0; j < 4; ++j) {
				if ((tile[0][j] == hint_tile && hint_tile == 3) || tile[0][j] + hint_tile == 3)
					reward_of_op[2] += factor;
				else if ((tile[1][j] == hint_tile && hint_tile == 3) || tile[1][j] + hint_tile == 3)
					reward_of_op[2] += secFactor;
			}
		} else if (op == 3) {
			for (int i = 0; i < 4; ++i) {
				if ((tile[i][3] == hint_tile && hint_tile == 3) || tile[i][3] + hint_tile == 3)
					reward_of_op[3] += factor;
				else if ((tile[i][2] == hint_tile and hint_tile == 3) || tile[i][2] + hint_tile == 3)
					reward_of_op[3] += secFactor;
			}
		} else if (op == 1) {
			for (int i = 0; i < 4; ++i) {
				if ((tile[i][0] == hint_tile and hint_tile == 3) || tile[i][0] + hint_tile == 3)
					reward_of_op[1] += factor;
				else if ((tile[i][1] == hint_tile and hint_tile == 3) || tile[i][1] + hint_tile == 3)
					reward_of_op[1] += secFactor;
			}
		}
	}

	virtual action take_action(const board& before) {
		count_move += 1;
		board::reward reward_of_op[4] = {0}; // reward for each op
		board::grid tile = board(before).getTile(); // board tile
		board::data attr = board(before).getAttr(); // board attr
		unsigned int hint_tile = attr & 3; // hint tile number
		int op = 0; // opcode
		
		for (int op : opcode)
			reward_of_op[op] = board(before).slide(op);

		if (reward_of_op[2] != -1 && reward_of_op[3] != -1) {
			add_reward_by_hint(tile, reward_of_op, hint_tile, 2, count_move);
			add_reward_by_hint(tile, reward_of_op, hint_tile, 3, count_move);
			op = reward_of_op[2] > reward_of_op[3] ? 2 : 3;
			return action::slide(op);
		} else if (reward_of_op[3] != -1)
			return action::slide(3);
		else if (reward_of_op[2] != -1 && reward_of_op[1] != -1) {
			add_reward_by_hint(tile, reward_of_op, hint_tile, 2, count_move);
			add_reward_by_hint(tile, reward_of_op, hint_tile, 1, count_move);
			op = reward_of_op[2] > reward_of_op[1] ? 2 : 1;
			return action::slide(op);
		}
		else if (reward_of_op[2] != -1)
			return action::slide(2);
		else if (reward_of_op[1] != -1)
			return action::slide(1);
		else
			return action::slide(0);

		return action();
	}

private:
	std::array<int, 4> opcode;
};
