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

#define N 46
#define Gamma 0.99
#define lambda 0.5

typedef struct {
	int states[N];
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

	void getState (state_t &state, board::grid tile) {
        /*
         *   o o x x
         *   o o x x
         *   x x x x    x 9
         *   x x x x
         */
        int tmp = 0, count = 0;
        for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
                tmp = 0;
                for (int k = i; k < i + 2; ++k) {
                    for (int l = j; l < j + 2; ++l) {
				        tmp += tile[k][l];
				        tmp *= 11;
                    }
			    }
		        state.states[count] = tmp / 11;
                count++;
            }
		}

		/*
         *   o o o x
         *   o o o x
         *   x x x x    x 6
         *   x x x x
         */
        for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 2; ++j) {
                tmp = 0;
                for (int k = i; k < i + 2; ++k) {
                    for (int l = j; l < j + 3; ++l) {
				        tmp += tile[k][l];
				        tmp *= 11;
                    }
			    }
		        state.states[count] = tmp / 11;
                count++;
            }
		}

        /*
         *   o o x x
         *   o o x x
         *   o o x x   x 6
         *   x x x x
         */
        for (int i = 0; i < 2; ++i) {
			for (int j = 0; j < 3; ++j) {
                tmp = 0;
                for (int k = i; k < i + 3; ++k) {
                    for (int l = j; l < j + 2; ++l) {
				        tmp += tile[k][l];
				        tmp *= 11;
                    }
			    }
		        state.states[count] = tmp / 11;
                count++;
            }
        }
		
        /*  
         *           k = 2:                         k = 0:
         *   o x x x    x o x x     x x o x     o o x x    x o o x     x x o o
         *   o x x x    x o x x     x x o x     o o x x    x o o x     x x o o
         *   o o x x    x o o x     x x o o     o x x x    x o x x     x x o x
         *   o o x x    x o o x     x x o o     o x x x    x o x x     x x o x
         */
        for (int k = 0; k < 3; k += 2) {
            for (int j = 0; j < 3; ++j) {
                tmp = 0;
                for (int i = 0; i < 4; ++i) {
	    		    tmp += tile[i][j];
		    	    tmp *= 11;
			    }
                tmp += tile[k][j + 1];
    			tmp *= 11;
                tmp += tile[k + 1][j + 1];
		        state.states[count] = tmp;
                count++;
            }
        }

        /*
         *           k = 2:                             k = 0:
         *   x x x o    x x o x     x o x x     x x o o    x o o x     o o x x
         *   x x x o    x x o x     x o x x     x x o o    x o o x     o o x x
         *   x x o o    x o o x     o o x x     x x x o    x x o x     x o x x
         *   x x o o    x o o x     o o x x     x x x o    x x o x     x o x x
         */
        for (int k = 0; k < 3; k += 2) {
            for (int j = 3; j > 0; --j) {
                tmp = 0;
                for (int i = 0; i < 4; ++i) {
	    		    tmp += tile[i][j];
		    	    tmp *= 11;
			    }
                tmp += tile[k][j - 1];
	    		tmp *= 11;
                tmp += tile[k + 1][j - 1];
		        state.states[count] = tmp;
                count++;
            }
        }

        /*  
         *           k = 0:                         k = 2:
         *   o o o o    x x x x     x x x x     o o o o    x x x x     x x x x
         *   o o x x    o o o o     x x x x     x x o o    o o o o     x x x x
         *   x x x x    o o x x     o o o o     x x x x    x x o o     o o o o
         *   x x x x    x x x x     o o x x     x x x x    x x x x     x x o o
         */
        for (int k = 0; k < 3; k += 2) {
            for (int i = 0; i < 3; ++i) {
                tmp = 0;
                for (int j = 0; j < 4; ++j) {
	    		    tmp += tile[i][j];
		    	    tmp *= 11;
			    }
                tmp += tile[i + 1][k];
    			tmp *= 11;
                tmp += tile[i + 1][k + 1];
		        state.states[count] = tmp;
                count++;
            }
        }

        /*  
         *           k = 0:                         k = 2:
         *   o o x x    x x x x     x x x x     x x o o    x x x x     x x x x
         *   o o o o    o o x x     x x x x     o o o o    x x o o     x x x x
         *   x x x x    o o o o     o o x x     x x x x    o o o o     x x o o
         *   x x x x    x x x x     o o o o     x x x x    x x x x     o o o o
         */
        for (int k = 0; k < 3; k += 2) {
            for (int i = 1; i < 4; ++i) {
                tmp = 0;
                for (int j = 0; j < 4; ++j) {
	    		    tmp += tile[i][j];
		    	    tmp *= 11;
			    }
                tmp += tile[i - 1][k];
    			tmp *= 11;
                tmp += tile[i - 1][k + 1];
		        state.states[count] = tmp;
                count++;
            }
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
			getState(tmpState, tile);
			tmpState.states[N - 1] = ((int) board(before).getAttr() & 0x3) - 1;
			float value = forward(tmpState); 
			if (value + tmp > maxValue) {
				state = tmpState;
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

    bool have384 (board before) {
        board::grid tile = before.getTile();
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (tile[i][j] >= 10)
                    return true;
            }
        }
        return false;
    }

	bool have192 (board before) {
        board::grid tile = before.getTile();
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (tile[i][j] >= 9)
                    return true;
            }
        }
        return false;
    }

	virtual action take_action(const board& before) {
		board::reward reward = 0;
		state_t state;

        if (have384(before)) {
            if (board(before).slide(0) != -1)
		    	return action::slide(0);
	        else if (board(before).slide(3) != -1)
			    return action::slide(3);
		    else if (board(before).slide(1) != -1)
		    	return action::slide(1);
    		else if (board(before).slide(2) != -1)
	    		return action::slide(2);
		    else
			    return action();
        }

		int maxOp = choose_max_value_action(reward, state, before);	
		
        if (maxOp == -1) {
            if (!have384(before)) {
                int tmp = rewards.back();
                tmp -= 9999;
                rewards.pop_back();
                rewards.push_back(tmp);
            }
            return action();
        }

        board after = board(before);
        after.slide(maxOp);
        if (!have384(before) && have384(after))
            reward += 9999;

		states.push_back(state);
		rewards.push_back(reward);
		return action::slide(maxOp);
	}

	void train (board::reward next_reward, state_t next_state, state_t state) {
		// 𝚯[𝝓(𝒔′_t)] ← 𝚯[𝝓(𝒔′_t)] + 𝜶(𝒓_t + 𝑽(𝒔′_{t+1}) − 𝑽(𝒔′_t))
		for (int i = 0; i < N; ++i)
			net[i].value[state.states[i]] += (alpha * (next_reward + Gamma * forward(next_state) - forward(state)));
	}

	void train_2step (board::reward next_reward, board::reward next_next_reward, state_t next_next_state, state_t state) {
		// 𝚯[𝝓(𝒔′_t)] ← 𝚯[𝝓(𝒔′_t)] + 𝜶(𝒓_t + r_{t+1} + 𝑽(𝒔′_{t+2}) − 𝑽(𝒔′_t))
		for (int i = 0; i < N; ++i)
			net[i].value[state.states[i]] += (alpha * (next_reward + Gamma * next_next_reward + Gamma * Gamma * forward(next_next_state) - forward(state)));
	}

	void train_lambda (std::vector <board::reward> rewards, std::vector <state_t> states, int last) {
		float q_target = 0.0; 
		for (int i = 0; i < last; ++i) {
			float sum = 0;
			for (int j = 0; j <= i; ++j) {
				sum += (pow(Gamma, j) * rewards[rewards.size() - 5 + j]);
			}
			sum += (pow(Gamma, i + 1) * forward(states[states.size() - 5 + i]));
			q_target += (sum * pow(lambda, i + 1));
		}
		for (int i = 0; i < N; ++i)
			net[i].value[states[states.size() - 6].states[i]] += (alpha * (q_target - forward(states[states.size() - 6])));		
	}

	void TD_0 () {
		// train last afterstate
		state_t next_next_state = states.back();
		board::reward next_next_reward = rewards.back();
		for (int i = 0; i < N; ++i)
			net[i].value[next_next_state.states[i]] -= (alpha * forward(next_next_state));

		states.pop_back();
		rewards.pop_back();
		// train last 2 afterstate
		state_t next_state = states.back();
		board::reward next_reward = rewards.back();
		for (int i = 0; i < N; ++i)
			net[i].value[next_state.states[i]] += (alpha * (next_reward - forward(next_state)));

		while (!states.empty()) {
			train_2step(next_reward, next_next_reward, next_state, states.back());
			next_next_state = next_state;
			next_next_reward = next_reward;
			next_state = states.back();
			next_reward = rewards.back();
			states.pop_back();
			rewards.pop_back();
		}
	}

	void TD_lambda () {
		state_t tmp;
		for (int i = 0; i < 5; ++i) {
			rewards.push_back(0);
			states.push_back(tmp);
		}

		for (int i = 0; i < 5; ++i) {
			train_lambda(rewards, states, i);
			states.pop_back();
			rewards.pop_back();
		}

		while (states.size() > 5) {
			train_lambda(rewards, states, 5);
			states.pop_back();
			rewards.pop_back();
		}
	}

	virtual void close_episode(const std::string& flag = "") {
        if (states.empty())
            return;

		// TD_0();
		TD_lambda();		
		states.clear();
		rewards.clear();
	}

private:
	std::vector <state_t> states;
	std::vector <board::reward> rewards;
};