#!/usr/bin/env python3

import sys
import copy

INF = float('inf')

class Router:
    def __init__(self, name):
        self.name = name
        self.neighbours = {}  # neighbor -> cost
        self.distance_table = {}  # destination -> via -> cost
        self.routing_table = {}   # destination -> (next_hop, cost)

    def initialize_table(self, routers):
        for dest in routers:
            if dest == self.name:
                continue
            self.distance_table[dest] = {}
            for via in routers:
                self.distance_table[dest][via] = INF
            if dest in self.neighbours:
                self.distance_table[dest][dest] = self.neighbours[dest]

    def update_table(self, routers):
        updated = False
        for dest in routers:
            if dest == self.name:
                continue
            min_cost = INF
            min_via = None
            for neighbor in self.neighbours:
                cost_to_neighbor = self.neighbours[neighbor]
                neighbor_table = via_router[neighbor].distance_table.get(dest, {})
                neighbor_cost = neighbor_table.get(dest, INF)
                total_cost = cost_to_neighbor + neighbor_cost

                if total_cost < min_cost or (total_cost == min_cost and (min_via is None or neighbor < min_via)):
                    min_cost = total_cost
                    min_via = neighbor

            prev_cost = self.routing_table.get(dest, (None, INF))[1]
            if prev_cost != min_cost:
                updated = True
                self.routing_table[dest] = (min_via, min_cost if min_via else INF)

            # 同时更新 distance table，记录各 via 的信息（主要记录最优 via）
            for via in self.neighbours:
                self.distance_table[dest][via] = INF
            if min_via:
                self.distance_table[dest][min_via] = min_cost

        return updated

    def print_distance_table(self, t, routers):
        print(f"Distance Table of router {self.name} at t={t}:")
        heads = sorted([r for r in routers if r != self.name])
        print("   " + "  ".join(heads))
        for to in heads:
            row = []
            for via in heads:
                cost = self.distance_table[to].get(via, INF)
                row.append("INF" if cost == INF else str(cost))
            print(f"{to}  " + "  ".join(row))
        print()

    def print_routing_table(self, routers):
        print(f"Routing Table of router {self.name}:")
        for dest in sorted([r for r in routers if r != self.name]):
            if dest in self.routing_table:
                nh, cost = self.routing_table[dest]
                if nh is not None and cost < INF:
                    print(f"{dest},{nh},{cost}")
                else:
                    print(f"{dest},INF,INF")
            else:
                print(f"{dest},INF,INF")
        print()

def parse_input():
    lines = [line.strip() for line in sys.stdin if line.strip() != '']
    idx = 0

    routers = []
    while lines[idx] != "START":
        routers.append(lines[idx])
        idx += 1
    idx += 1

    links = []
    while lines[idx] != "UPDATE":
        parts = lines[idx].split()
        links.append((parts[0], parts[1], int(parts[2])))
        idx += 1
    idx += 1

    updates = []
    while lines[idx] != "END":
        parts = lines[idx].split()
        updates.append((parts[0], parts[1], int(parts[2])))
        idx += 1

    return routers, links, updates

def run_dv(routers):
    t = 0
    while True:
        for r in sorted(routers):
            via_router[r].print_distance_table(t, routers)
        updated = False
        for r in routers:
            updated |= via_router[r].update_table(routers)
        if not updated:
            break
        t += 1

    for r in sorted(routers):
        via_router[r].print_routing_table(routers)

# --- MAIN ---
routers, links, updates = parse_input()
via_router = {name: Router(name) for name in routers}

# Apply initial links
for a, b, cost in links:
    if cost != -1:
        via_router[a].neighbours[b] = cost
        via_router[b].neighbours[a] = cost

# Init tables
for r in routers:
    via_router[r].initialize_table(routers)

# Run Distance Vector
run_dv(routers)

# Apply updates
for a, b, cost in updates:
    if cost == -1:
        via_router[a].neighbours.pop(b, None)
        via_router[b].neighbours.pop(a, None)
    else:
        via_router[a].neighbours[b] = cost
        via_router[b].neighbours[a] = cost

# Re-init and rerun
for r in routers:
    via_router[r].initialize_table(routers)
    via_router[r].routing_table.clear()

run_dv(routers)
