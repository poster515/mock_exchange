from typing import List
import sys
import signal
import time

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



if __name__ == '__main__':

    agents: List[AlpacaAgent] = []
    agents.append(MeanReversionClient())
    agents.append(MomentumAgent())
    agents.append(OpenCloseAgent())

    for agent in agents:
        agent.start()

    while RUN:
        # Returns current time as a float (seconds since Jan 1, 1970)
        current_epoch: float = time.time()
        for agent in agents:
            agent.execute_strategy(current_epoch)

    print("Agent runner shutting down...")
    sys.exit(0)
