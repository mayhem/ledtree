#!/usr/bin/env python

# #discover <last id> <level>
# @<id> <cmd> <args>

CHILD0 = 0
CHILD1 = 1
PARENT = 2

class Node(object):

    def __init__(self):
        self.id = 0
        self.child0 = None
        self.child1 = None
        self.child0_id = 0
        self.child1_id = 0
        self.level = 0

    def message(self, cmd):
        if cmd.startswith("#discover"):
            return self.discover(cmd)
        else:
            data = cmd[1:].split(' ')
            dest = int(data[0])
            action = data[1]
            if dest == self.id:
                print "%d: process %s" % (self.id, cmd)
                return ""

            if self.child0 and dest <= self.child0_id:
                return self.child0.message(cmd)
            if self.child1 and dest >= self.child0_id:
                return self.child1.message(cmd)

            return "WTF"

    def discover(self, cmd):
        self.id, self.level = cmd[10:].split(" ")
        self.id = int(self.id)
        self.id += 1
        self.level = int(self.level)
        print "discovered: %d at level %d" % (self.id, self.level)

        cmd = "#discover %d %d" % (self.id, self.level + 1)
        if self.child0:
            cmd = self.child0.message(cmd)
            self.child0_id = int(cmd[10:].split(" ")[0])

        if self.child1:
            cmd = self.child1.message(cmd)
            self.child1_id = int(cmd[10:].split(" ")[0])

        p = cmd.split(" ")
        p[2] = str(self.level)
        cmd = " ".join(p)

        return cmd

def build_tree(level, max_levels):
    node = Node()
    if level == max_levels: return node

    node.child0 = build_tree(level + 1, max_levels)
    node.child1 = build_tree(level + 1, max_levels)

    return node

root = build_tree(0, 3)
node_count = int(root.message("#discover 0 0").split(' ')[1])
print "Found %d nodes" % node_count
print root.message("@15 level")
