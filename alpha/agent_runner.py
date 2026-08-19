from typing import List
import sys
import signal
import time

import argparse

from alpha.agents.mean_reversion import MeanReversionClient
from alpha.agents.momentum import MomentumAgent
from alpha.agents.open_close import OpenCloseAgent
from alpha.agents.agent import AlpacaAgent

RUN = True

def signal_handler(sig, frame):
    print('You pressed Ctrl+C!')
    global RUN
    RUN = False


signal.signal(signal.SIGINT, signal_handler)

class Runner:
    AGENT_DICT = dict([(a.__class__.__name__, a) for a in [MeanReversionClient, MomentumAgent, OpenCloseAgent]])

    @classmethod
    def run_agent(cls):
        print(f"Runner has the following map: {cls.AGENT_DICT}")

        agent = cls.AGENT_DICT[cls.AGENT]()
        while RUN:
            # Returns current time as a float (seconds since Jan 1, 1970)
            current_epoch: float = time.time()
            agent.execute_strategy(current_epoch)


if __name__ == '__main__':

    parser = argparse.ArgumentParser(prog='AgentRunner', description='Runs a trading strategy agent')
    parser.add_argument('--agent_name')
    args = parser.parse_args(args=["--agent_name", "AGENT"], namespace=Runner)
    print(args.filename, args.count, args.verbose)




    agents: List[AlpacaAgent] = []
    agents.append(MeanReversionClient())
    agents.append(MomentumAgent())
    agents.append(OpenCloseAgent())

    for agent in agents:
        agent.start()

    

    print("Agent runner shutting down...")
    sys.exit(0)
