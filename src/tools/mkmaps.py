#!/usr/bin/env python3
"""Generates the hand-designed town maps as ASCII map files (assets/data/maps/*.txt).
Run from the project root: python3 tools/mkmaps.py
The generated files are plain text and can be edited by hand afterwards."""
import random, os

OUT = os.path.join(os.path.dirname(__file__), '..', 'assets', 'data', 'maps')

class M:
    def __init__(self, w, h, fill, seed=1):
        self.w, self.h = w, h
        self.g = [[fill] * w for _ in range(h)]
        self.objs = []
        self.rng = random.Random(seed)
    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h: self.g[y][x] = c
    def get(self, x, y): return self.g[y][x] if 0 <= x < self.w and 0 <= y < self.h else ' '
    def fill(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w): self.set(xx, yy, c)
    def rect(self, x, y, w, h, c):
        for xx in range(x, x + w): self.set(xx, y, c); self.set(xx, y + h - 1, c)
        for yy in range(y, y + h): self.set(x, yy, c); self.set(x + w - 1, yy, c)
    def building(self, x, y, w, h, wall='#', floor='h', door=None, windows=True, sign=None):
        self.fill(x, y, w, h, floor); self.rect(x, y, w, h, wall)
        if door is None: door = (x + w // 2, y + h - 1)
        self.set(door[0], door[1], 'D')
        if windows and w > 4:
            self.set(x + 1, y + h - 1, 'W'); self.set(x + w - 2, y + h - 1, 'W')
        if sign: self.set(door[0] - 1 if door[0] - 1 != x else door[0] + 1, door[1], sign)
        return door
    def hline(self, x0, x1, y, c):
        for x in range(min(x0, x1), max(x0, x1) + 1): self.set(x, y, c)
    def vline(self, x, y0, y1, c):
        for y in range(min(y0, y1), max(y0, y1) + 1): self.set(x, y, c)
    def scatter(self, c, n, allowed, region=None):
        rx, ry, rw, rh = region or (0, 0, self.w, self.h)
        for _ in range(n):
            x, y = self.rng.randrange(rx, rx + rw), self.rng.randrange(ry, ry + rh)
            if self.get(x, y) in allowed: self.set(x, y, c)
    def border(self, c):
        self.rect(0, 0, self.w, self.h, c)
    def obj(self, kind, **kv):
        if kind == 'exit':  # exits must be walkable: carve road tiles under them
            for yy in range(kv['y'], kv['y'] + kv.get('h', 1)):
                for xx in range(kv['x'], kv['x'] + kv.get('w', 1)): self.set(xx, yy, 'r')
        if kind in ('npc', 'chest', 'savepoint', 'enemy'):
            c = self.get(kv['x'], kv['y'])
            if c not in '.,rhmR~F+': self.set(kv['x'], kv['y'], self.floor_for(kv['x'], kv['y']))
        self.objs.append((kind, kv))
    def floor_for(self, x, y):
        # pick a sensible floor from the neighbours
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c = self.get(x + dx, y + dy)
            if c in '.,rhmR~F': return c
        return '.'
    def spawn_fix(self, x, y): self.set(x, y, 'r')
    def write(self, path, header, legend):
        lines = ['[map]'] + [f'{k}={v}' for k, v in header.items()] + ['', '[legend]']
        lines += [f'{k} = {v}' for k, v in legend.items()] + ['', '[rows]']
        lines += [''.join(r) for r in self.g] + ['']
        for kind, kv in self.objs:
            lines.append(f'[{kind}]')
            lines += [f'{k}={v}' for k, v in kv.items()]
            lines.append('')
        with open(path, 'w') as f: f.write('\n'.join(lines))
        print('wrote', path)

COMMON = {
    '.': 'floor_grass', ',': 'floor_dirt', 'r': 'road', 'T': 'tree|tree|tree2', 't': 'pine_t', 'f': 'fence_wood', 'F': 'flower|flower2|floor_grass',
    '#': 'wall_wood', '=': 'wall_stone', 'h': 'floor_wood', 'm': 'floor_tile', 'D': 'door_wood', 'W': 'window', 'o': 'well', 'w': 'floor_water',
    'b': 'barrel', 'B': 'bed', 'A': 'table', 'C': 'chair', 'S': 'shelf', 'K': 'bookshelf', 'n': 'anvil', 'M': 'forge', 'u': 'cauldron',
    's': 'shop_sign', 'i': 'inn_sign', 'p': 'pillar', 'x': 'void', 'G': 'grave', 'a': 'altar', 'c': 'crop|crop2', 'H': 'hay', 'k': 'crate',
    '%': 'banner_red', '&': 'banner_blue', '$': 'banner_gold', 'Q': 'fountain', 'L': 'torch', 'Z': 'statue', 
    'R': 'floor_marble', 'e': 'fence', '~': 'bridge', '+': 'floor_carpet', 'O': 'brazier', 'v': 'floor_water', 'j': 'deadtree_t', 'y': 'bush',
}
MODERN = {
    '.': 'floor_concrete', ',': 'floor_road', 'r': 'road_line', '#': 'wall_concrete', '=': 'wall_metal', 'h': 'floor_tile', 'm': 'floor_metal',
    'D': 'door_metal', 'W': 'window', 'k': 'crate', 'b': 'box', 'x': 'void', 'c': 'car|car2', 'C': 'car_red', 'u': 'truck', 'l': 'streetlamp',
    'g': 'trafficlight', 'S': 'sign_stop', 'z': 'sign_warn', 'P': 'pipe_v', 'p': 'pipe_h', 'V': 'vending', 'E': 'terminal', 'e': 'terminal2',
    'v': 'server', 'K': 'computer', 'T': 'deadtree_t', 't': 'trash', '~': 'rubble', 'R': 'riftscar', 'a': 'altar', 'B': 'building',
    'n': 'building2', 'N': 'building3', 'o': 'building4', 'O': 'building_red', 'L': 'torch', 'Y': 'cryopod', 'I': 'reactor', 'w': 'floor_water',
    'F': 'floor_grid', 'j': 'satellite', 'J': 'rocket_wreck', 'U': 'ufo', 'y': 'wall_hull', 'A': 'table', 'q': 'chair', 'f': 'fence', 'G': 'grave',
}

def brasswick():
    m = M(60, 44, '.', seed=3)
    m.border('T')
    m.scatter('T', 60, '.', (1, 1, 58, 4)); m.scatter('T', 60, '.', (1, 39, 58, 4)); m.scatter('T', 30, '.', (1, 1, 4, 42)); m.scatter('T', 30, '.', (55, 1, 4, 42))
    m.scatter('F', 40, '.')
    # city wall
    m.rect(5, 5, 50, 34, '=')
    m.hline(26, 33, 38, 'r'); m.set(26, 38, '='); m.set(33, 38, '=')   # south gate
    m.hline(27, 32, 5, 'r'); m.set(26, 5, '='); m.set(33, 5, '=')        # north gate
    m.vline(5, 20, 24, 'r'); m.set(5, 20, '='); m.set(5, 24, '=')        # west gate
    m.vline(54, 20, 24, 'r'); m.set(54, 20, '='); m.set(54, 24, '=')     # east gate
    # streets
    m.fill(27, 6, 6, 32, 'r'); m.fill(6, 21, 48, 3, 'r')
    m.fill(27, 0, 6, 6, 'r'); m.fill(27, 38, 6, 6, 'r'); m.fill(0, 21, 6, 3, 'r'); m.fill(54, 21, 6, 3, 'r')
    m.fill(10, 12, 40, 2, 'r'); m.fill(10, 30, 40, 2, 'r'); m.vline(10, 12, 31, 'r'); m.vline(49, 12, 31, 'r'); m.vline(11, 12, 31, 'r'); m.vline(48, 12, 31, 'r')
    # plaza
    m.fill(24, 16, 12, 10, 'r'); m.set(29, 20, 'Q'); m.set(30, 20, 'Q'); m.set(29, 21, 'Q'); m.set(30, 21, 'Q')
    m.set(24, 16, 'p'); m.set(35, 16, 'p'); m.set(24, 25, 'p'); m.set(35, 25, 'p')
    # buildings: town hall (north of plaza)
    m.building(8, 6, 16, 6, wall='=', floor='m', door=(16, 11))
    m.set(9, 11, '%'); m.set(22, 11, '%'); m.fill(10, 7, 12, 1, 'K')
    # keep (NE)
    m.building(40, 6, 12, 10, wall='=', floor='R', door=(45, 15)); m.set(44, 15, '$'); m.set(46, 15, '$')
    m.set(41, 7, 'L'); m.set(50, 7, 'L'); m.set(45, 7, 'Z')
    # armory (west of plaza)
    m.building(13, 15, 9, 6, door=(17, 20), sign='s'); m.set(14, 16, 'n'); m.set(15, 16, 'M'); m.set(20, 16, 'S')
    # market stalls (south-west)
    m.building(13, 25, 9, 5, door=(17, 29), sign='s'); m.set(14, 26, 'S'); m.set(20, 26, 'S'); m.set(14, 28, 'b')
    # arcane shop (east of plaza)
    m.building(38, 17, 9, 5, door=(42, 21), sign='s'); m.set(39, 18, 'K'); m.set(45, 18, 'K'); m.set(42, 18, 'u')
    # inn (south-east)
    m.building(38, 25, 11, 6, door=(42, 30), sign='i'); m.set(39, 26, 'B'); m.set(47, 26, 'B'); m.set(43, 26, 'A'); m.set(44, 26, 'A'); m.set(39, 28, 'b')
    # temple (south, west of gate)
    m.building(13, 32, 11, 5, wall='=', floor='m', door=(18, 36)); m.set(18, 33, 'a'); m.set(14, 33, 'L'); m.set(22, 33, 'L')
    # houses (south-east)
    m.building(38, 33, 6, 4, door=(40, 36)); m.building(45, 33, 6, 4, door=(47, 36))
    # houses W
    m.building(6, 26, 6, 4, door=(8, 29)); m.building(6, 33, 6, 4, door=(8, 36))
    # guard barracks (west of west gate)
    m.building(6, 14, 6, 6, wall='=', floor='m', door=(8, 19)); m.set(7, 15, 'B'); m.set(10, 15, 'B'); m.set(7, 17, 'k')
    # trees inside
    m.scatter('T', 10, '.', (6, 6, 48, 32)); m.scatter('y', 14, '.', (6, 6, 48, 32))
    leg = dict(COMMON); leg['R'] = 'floor_marble'
    m.obj('exit', x=27, y=43, w=6, h=1, to='overworld', tx=84, ty=71)
    m.obj('exit', x=27, y=0, w=6, h=1, to='overworld', tx=84, ty=69)
    m.obj('exit', x=0, y=20, w=1, h=5, to='overworld', tx=83, ty=70)
    m.obj('exit', x=59, y=20, w=1, h=5, to='overworld', tx=85, ty=70)
    m.obj('npc', id='mayor', name='Mayor Aldous', sprite='104,10', x=16, y=8, dialogue='mayor_aldous')
    m.obj('npc', id='clerk', name='Clerk Benning', sprite='104,0', x=11, y=9, dialogue='clerk_benning')
    m.obj('npc', id='reyna', name='Captain Reyna', sprite='110,18', x=8, y=16, dialogue='captain_reyna')
    m.obj('npc', id='guard1', name='Gate Guard', sprite='110,18', x=28, y=37, dialogue='gate_guard')
    m.obj('npc', id='guard2', name='Keep Guard', sprite='110,18', x=45, y=16, dialogue='keep_guard')
    m.obj('npc', id='armorer', name='Armorer Hald', sprite='106,11', x=17, y=17, dialogue='armorer_hald')
    m.obj('npc', id='pell', name='Merchant Pell', sprite='105,1', x=17, y=27, dialogue='merchant_pell')
    m.obj('npc', id='elowen', name='Elowen', sprite='109,28', x=43, y=19, dialogue='elowen', hide='joined_mage')
    m.obj('npc', id='arcanist', name='Arcanist Morrow', sprite='104,28', x=40, y=19, dialogue='arcanist_morrow')
    m.obj('npc', id='innkeeper2', name='Innkeeper Fenna', sprite='104,1', x=43, y=28, dialogue='innkeeper_fenna')
    m.obj('npc', id='priest', name='Brother Caul', sprite='104,26', x=17, y=34, dialogue='brother_caul')
    m.obj('npc', id='townsfolk1', name='Washerwoman', sprite='104,1', x=20, y=23, dialogue='townsfolk', wander=1)
    m.obj('npc', id='townsfolk2', name='Street Urchin', sprite='104,0', x=33, y=23, dialogue='urchin', wander=1)
    m.obj('npc', id='townsfolk3', name='Veteran', sprite='105,0', x=36, y=22, dialogue='veteran', wander=1)
    m.obj('npc', id='bard', name='Bard Lio', sprite='104,21', x=31, y=27, dialogue='bard_lio')
    m.obj('npc', id='cat', name='Cat', sprite='1,14', x=9, y=27, dialogue='cat', wander=1)
    m.obj('sign', x=29, y=36, text='BRASSWICK - Free City. Keep your blade sheathed inside the walls.')
    m.obj('sign', x=23, y=20, text='Hald\'s Armory - steel for every purse.')
    m.obj('sign', x=37, y=21, text='The Gilded Circle - arcane supplies.')
    m.obj('sign', x=37, y=30, text='The Copper Kettle Inn - beds 15 gold.')
    m.obj('chest', x=50, y=7, flag='bw_keep_chest', items='steel_sword:1,potion_red2:1', gold=60, locked='mayor_seal')
    m.obj('chest', x=20, y=18, flag='bw_armory_chest', items='arrows:30')
    m.obj('savepoint', x=29, y=18)
    m.spawn_fix(30, 41)
    m.write(os.path.join(OUT, 'brasswick.txt'), dict(id='brasswick', name='Brasswick', kind='town', spawn='30,41', bg='20,40,20'), leg)

def millbrook():
    m = M(36, 26, '.', seed=5)
    m.border('T'); m.scatter('T', 30, '.', (1, 1, 34, 3)); m.scatter('F', 20, '.')
    m.fill(2, 12, 32, 2, 'r')
    m.fill(4, 15, 12, 7, 'c'); m.rect(3, 14, 14, 9, 'f'); m.set(10, 14, '.')
    m.building(6, 4, 8, 5, door=(9, 8)); m.set(7, 5, 'B'); m.set(12, 5, 'A')
    m.building(20, 3, 10, 6, door=(24, 8)); m.set(21, 4, 'H'); m.set(28, 4, 'H'); m.set(21, 7, 'H')
    m.fill(22, 16, 8, 5, '.'); m.rect(21, 15, 10, 7, 'f'); m.set(26, 15, '.')
    m.set(18, 10, 'o')
    m.obj('exit', x=1, y=12, w=1, h=2, to='overworld', tx=59, ty=96)
    m.obj('exit', x=34, y=12, w=1, h=2, to='overworld', tx=61, ty=96)
    m.obj('npc', id='jory', name='Trapper Jory', sprite='117,27', x=24, y=6, dialogue='trapper_jory')
    m.obj('npc', id='nyx', name='Nyx', sprite='117,10', x=24, y=11, dialogue='nyx', hide='joined_ranger')
    m.obj('npc', id='farmer', name='Goodwife Ada', sprite='104,1', x=9, y=6, dialogue='goodwife_ada')
    m.obj('npc', id='cow', name='Cow', sprite='0,16', x=25, y=18, dialogue='cow', wander=1)
    m.obj('npc', id='pig', name='Pig', sprite='2,16', x=27, y=19, dialogue='pig', wander=1)
    m.obj('npc', id='chicken', name='Chicken', sprite='6,16', x=23, y=17, dialogue='chicken', wander=1)
    m.obj('chest', x=12, y=7, flag='mb_chest', items='bread:3,cheese:1')
    m.obj('sign', x=17, y=11, text='MILLBROOK FARM. Beware of the goose.')
    m.spawn_fix(2, 13)
    m.write(os.path.join(OUT, 'millbrook.txt'), dict(id='millbrook', name='Millbrook Farm', kind='town', spawn='2,13', bg='20,40,20'), dict(COMMON))

def hermit():
    m = M(24, 20, '.', seed=7)
    m.border('T'); m.scatter('T', 40, '.', (1, 1, 22, 18)); m.scatter('j', 6, '.')
    m.fill(8, 6, 8, 8, '.'); m.building(9, 7, 6, 5, door=(11, 11)); m.set(10, 8, 'B'); m.set(13, 8, 'u'); m.set(13, 9, 'K')
    m.hline(11, 11, 12, '.'); m.vline(11, 12, 18, ','); m.fill(10, 17, 4, 2, ',')
    m.obj('exit', x=10, y=19, w=4, h=1, to='overworld', tx=96, ty=91)
    m.obj('npc', id='hermit', name='The Hermit', sprite='104,23', x=12, y=9, dialogue='hermit')
    m.obj('chest', x=10, y=10, flag='hermit_chest', items='potion_blue:1,potion_red:2')
    m.obj('savepoint', x=14, y=10)
    m.spawn_fix(11, 18)
    m.write(os.path.join(OUT, 'hermit_hut.txt'), dict(id='hermit_hut', name="Hermit's Hut", kind='town', spawn='11,18', bg='10,25,10'), dict(COMMON))

def murkwater():
    m = M(40, 30, '.', seed=9)
    m.border('T'); m.scatter('w', 120, '.'); m.scatter('j', 25, '.'); m.scatter('T', 20, '.')
    m.fill(4, 13, 32, 3, '~'); m.fill(14, 6, 14, 14, '.'); m.fill(18, 0, 6, 30, 'r')
    for (x, y) in [(6, 8), (6, 20), (30, 8), (30, 20)]:
        m.fill(x - 1, y - 1, 7, 7, '.'); m.building(x, y, 5, 5, door=(x + 2, y + 4))
    m.building(14, 2, 7, 5, door=(17, 6), sign='i'); m.building(23, 2, 7, 5, door=(26, 6), sign='s')
    m.fill(16, 22, 10, 4, '.'); m.set(20, 23, 'a')
    m.set(20, 15, 'o')
    m.obj('exit', x=19, y=29, w=4, h=1, to='overworld', tx=118, ty=109)
    m.obj('exit', x=18, y=0, w=6, h=1, to='overworld', tx=118, ty=107)
    m.obj('npc', id='fenwick', name='Old Fenwick', sprite='104,10', x=8, y=10, dialogue='old_fenwick')
    m.obj('npc', id='shrine_keeper', name='Keeper Aldra', sprite='104,26', x=21, y=24, dialogue='keeper_aldra')
    m.obj('npc', id='swamp_trader', name='Trader Mosk', sprite='105,1', x=26, y=4, dialogue='trader_mosk')
    m.obj('npc', id='swamp_inn', name='Innkeeper Ule', sprite='104,0', x=17, y=4, dialogue='innkeeper_ule')
    m.obj('npc', id='fisher', name='Fisherwoman Tam', sprite='104,1', x=32, y=22, dialogue='fisher_tam', wander=1)
    m.obj('npc', id='kid', name='Mud-caked Boy', sprite='104,0', x=22, y=12, dialogue='mud_boy', wander=1)
    m.obj('npc', id='frog', name='Frog', sprite='2,14', x=10, y=24, dialogue='frog', wander=1)
    m.obj('chest', x=32, y=10, flag='mw_chest', items='herb_moon:1,antidote:2')
    m.obj('sign', x=19, y=21, text='Shrine of the Drowned. Light a candle for those the marsh took.')
    m.obj('savepoint', x=21, y=25)
    m.spawn_fix(20, 28)
    m.write(os.path.join(OUT, 'murkwater.txt'), dict(id='murkwater', name='Murkwater', kind='town', spawn='20,28', bg='15,30,25'), dict(COMMON))

def sunscar():
    m = M(40, 28, '.', seed=11)
    leg = dict(COMMON); leg['.'] = 'floor_sand'; leg['T'] = 'cactus'; leg['y'] = 'dune'; leg['t'] = 'rocks'
    m.border('t'); m.scatter('T', 25, '.'); m.scatter('y', 50, '.')
    m.fill(6, 6, 28, 16, '.'); m.fill(18, 6, 4, 22, 'r')
    m.building(8, 8, 8, 5, wall='=', floor='m', door=(11, 12)); m.building(24, 8, 8, 5, door=(27, 12), sign='s')
    m.building(8, 16, 8, 5, door=(11, 20), sign='i'); m.building(24, 16, 8, 5, wall='=', floor='m', door=(27, 20))
    m.set(20, 14, 'o'); m.set(19, 23, 'O'); m.set(22, 23, 'O')
    m.obj('exit', x=18, y=27, w=4, h=1, to='overworld', tx=32, ty=117)
    m.obj('npc', id='sefa', name='Matriarch Sefa', sprite='104,22', x=11, y=9, dialogue='matriarch_sefa')
    m.obj('npc', id='raider_env', name='Raider Envoy Kesh', sprite='107,22', x=27, y=17, dialogue='raider_kesh')
    m.obj('npc', id='sun_trader', name='Trader Nabir', sprite='105,1', x=27, y=9, dialogue='trader_nabir')
    m.obj('npc', id='sun_inn', name='Innkeeper Ora', sprite='104,1', x=11, y=17, dialogue='innkeeper_ora')
    m.obj('npc', id='sun_kid', name='Sand Dancer', sprite='104,0', x=20, y=18, dialogue='sand_dancer', wander=1)
    m.obj('npc', id='camel', name='Goat', sprite='1,16', x=14, y=14, dialogue='goat', wander=1)
    m.obj('chest', x=14, y=9, flag='ss_chest', items='potion_red:2,charm_fire:1')
    m.obj('sign', x=16, y=24, text='SUNSCAR OUTPOST. Water is life. Do not waste it.')
    m.obj('savepoint', x=21, y=14)
    m.spawn_fix(20, 26)
    m.write(os.path.join(OUT, 'sunscar.txt'), dict(id='sunscar', name='Sunscar Outpost', kind='town', spawn='20,26', bg='90,70,40'), leg)

def frosthold():
    m = M(34, 28, '.', seed=13)
    leg = dict(COMMON); leg['.'] = 'snow'; leg['T'] = 'snowpine'; leg['t'] = 'rocks'
    m.border('t'); m.scatter('T', 60, '.')
    m.fill(10, 6, 14, 16, '.'); m.fill(15, 6, 4, 22, 'r')
    m.building(11, 7, 12, 8, wall='=', floor='m', door=(16, 14)); m.set(16, 9, 'a'); m.set(12, 8, 'L'); m.set(21, 8, 'L'); m.set(12, 13, 'O'); m.set(21, 13, 'O'); m.set(16, 8, 'Z')
    m.building(11, 17, 6, 5, door=(13, 21)); m.set(12, 18, 'B'); m.set(15, 18, 'B')
    m.building(19, 17, 6, 5, door=(21, 21)); m.set(20, 18, 'S'); m.set(23, 18, 'u')
    m.obj('exit', x=15, y=27, w=4, h=1, to='overworld', tx=46, ty=23)
    m.obj('npc', id='orin', name='Orin', sprite='104,26', x=16, y=11, dialogue='orin', hide='joined_cleric')
    m.obj('npc', id='abbot', name='Abbot Tyr', sprite='104,10', x=14, y=10, dialogue='abbot_tyr')
    m.obj('npc', id='pilgrim', name='Pilgrim', sprite='104,0', x=18, y=23, dialogue='pilgrim', wander=1)
    m.obj('npc', id='frost_trader', name='Sister Maud', sprite='104,1', x=22, y=19, dialogue='sister_maud')
    m.obj('chest', x=15, y=18, flag='fh_chest', items='charm_frost:1,potion_blue:1')
    m.obj('savepoint', x=17, y=16)
    m.spawn_fix(16, 26)
    m.write(os.path.join(OUT, 'frosthold.txt'), dict(id='frosthold', name='Frosthold Shrine', kind='town', spawn='16,26', bg='60,70,90'), leg)

def ashfall():
    m = M(60, 44, '.', seed=17)
    m.border('~'); m.scatter('~', 200, '.'); m.scatter('T', 30, '.'); m.scatter('R', 25, '.'); m.scatter('t', 20, '.')
    # roads
    m.fill(6, 20, 48, 4, ','); m.hline(6, 53, 22, 'r'); m.fill(28, 4, 4, 36, ','); m.vline(30, 4, 39, 'r')
    m.fill(10, 8, 40, 2, ','); m.fill(10, 34, 40, 2, ',')
    # ruined towers (building props) along the roads
    for x in range(8, 52, 6):
        m.set(x, 11, m.rng.choice('BnNoO')); m.set(x + 2, 31, m.rng.choice('BnNoO'))
    # cars
    for (x, y) in [(12, 21), (20, 23), (36, 21), (44, 23), (31, 10), (29, 30)]: m.set(x, y, 'c')
    m.set(50, 21, 'u'); m.set(16, 23, 'C')
    for (x, y) in [(10, 19), (26, 19), (40, 19), (54, 19), (10, 25), (26, 25), (40, 25), (54, 25)]: m.set(x, y, 'l')
    m.set(27, 19, 'g'); m.set(33, 25, 'g')
    # scavenger settlement in a half-standing building (NW)
    m.fill(8, 12, 16, 7, '.'); m.building(9, 12, 14, 7, wall='#', floor='h', door=(15, 18)); m.set(10, 13, 'V'); m.set(21, 13, 'k'); m.set(20, 13, 'k'); m.set(10, 16, 'A'); m.set(11, 16, 'q')
    # Tessa's workshop (NE)
    m.building(38, 12, 12, 7, wall='=', floor='m', door=(43, 18)); m.set(39, 13, 'E'); m.set(48, 13, 'v'); m.set(40, 16, 'K'); m.set(47, 16, 'b')
    # bar (SW)
    m.building(10, 26, 12, 6, wall='#', floor='h', door=(15, 31)); m.set(11, 27, 'b'); m.set(20, 27, 'b'); m.set(14, 28, 'A'); m.set(15, 28, 'A'); m.set(13, 28, 'q'); m.set(16, 28, 'q')
    # cult shrine ruin (SE)
    m.fill(40, 26, 10, 6, 'R'); m.set(44, 28, 'a'); m.set(42, 27, 'L'); m.set(47, 27, 'L')
    # lab gate marker south, wreck north
    m.set(30, 40, 'z'); m.set(30, 3, 'z')
    m.scatter('t', 10, '.'); m.scatter('b', 10, '.'); m.scatter('k', 8, '.')
    m.obj('exit', x=28, y=43, w=4, h=1, to='overworld', tx=112, ty=47)
    m.obj('exit', x=28, y=0, w=4, h=1, to='overworld', tx=112, ty=45)
    m.obj('exit', x=0, y=20, w=1, h=4, to='overworld', tx=111, ty=46)
    m.obj('exit', x=59, y=20, w=1, h=4, to='overworld', tx=113, ty=46)
    m.obj('npc', id='tessa', name='Tessa', sprite='116,15', x=43, y=15, dialogue='tessa', hide='joined_scavenger')
    m.obj('npc', id='scrap_vendor', name='Junk-Dealer Vosk', sprite='105,15', x=12, y=15, dialogue='junk_vosk')
    m.obj('npc', id='barkeep', name='Barkeep Rusty', sprite='104,13', x=18, y=29, dialogue='barkeep_rusty')
    m.obj('npc', id='scav1', name='Scavenger', sprite='104,15', x=17, y=14, dialogue='scavenger_talk', wander=1)
    m.obj('npc', id='scav2', name='Lookout', sprite='116,15', x=33, y=14, dialogue='lookout', wander=1)
    m.obj('npc', id='cultist_npc', name='Robed Figure', sprite='104,17', x=45, y=30, dialogue='robed_figure', hide='cult_fought')
    m.obj('npc', id='drone_pet', name='Pet Drone', sprite='111,43', x=45, y=16, dialogue='pet_drone', wander=1)
    m.obj('enemy', x=50, y=36, group='scrap_drone,scrap_drone', aggro=4, flag='af_drones1')
    m.obj('enemy', x=8, y=38, group='security_bot', aggro=4, flag='af_bot1')
    m.obj('chest', x=21, y=14, flag='af_chest1', items='ammo_pistol:20,medkit:1')
    m.obj('chest', x=48, y=16, flag='af_chest2', items='circuit:1,cell:10', gold=40)
    m.obj('chest', x=49, y=31, flag='af_chest3', items='scroll_lightning:1', gold=70)
    m.obj('sign', x=31, y=38, text='SOUTH: Research Annex - KEEP OUT (sign riddled with bullet holes)')
    m.obj('sign', x=31, y=4, text='NORTH: the Wreck. Nobody who went in came back the same.')
    m.obj('savepoint', x=41, y=14)
    m.spawn_fix(30, 41)
    m.write(os.path.join(OUT, 'ashfall.txt'), dict(id='ashfall', name='Ashfall', kind='ruins', spawn='30,41', bg='30,28,36', encounter='0'), dict(MODERN))

def wreck():
    m = M(50, 36, '.', seed=19)
    leg = dict(MODERN); leg['.'] = 'wasteland'; leg['~'] = 'rubble'
    m.border('~'); m.scatter('~', 150, '.'); m.scatter('T', 25, '.'); m.scatter('R', 30, '.')
    # hull shape
    m.fill(12, 8, 30, 18, 'y'); m.fill(14, 10, 26, 14, 'F'); m.set(27, 25, 'D'); m.set(27, 24, 'F')
    for x in range(16, 38, 5): m.set(x, 8, 'W')
    m.set(20, 12, 'Y'); m.set(32, 12, 'Y'); m.set(26, 12, 'I'); m.set(16, 20, 'v'); m.set(36, 20, 'v'); m.set(26, 18, 'E'); m.set(22, 15, 'k'); m.set(30, 15, 'k')
    m.set(26, 14, 'D')  # door down into lower decks (exit)
    m.set(8, 30, 'J'); m.set(42, 30, 'j'); m.set(44, 5, 'U')
    m.fill(25, 26, 4, 10, ',')
    m.obj('exit', x=23, y=35, w=6, h=1, to='overworld', tx=126, ty=29)
    m.obj('exit', x=26, y=14, to='ship', tx=0, ty=0, item='keycard_blue', locked='The hatch needs a blue keycard.')
    m.obj('npc', id='ship_ai', name='Dormant Android', sprite='108,43', x=27, y=18, dialogue='ship_android')
    m.obj('npc', id='ship_vendor', name='Quartermaster Unit', sprite='105,43', x=16, y=21, dialogue='ship_vendor')
    m.obj('enemy', x=36, y=12, group='scrap_drone,scrap_drone,security_bot', aggro=5, flag='wr_guard1')
    m.obj('enemy', x=18, y=16, group='alien,alien', aggro=5, flag='wr_guard2')
    m.obj('enemy', x=40, y=28, group='security_bot,scrap_drone', aggro=4, flag='wr_guard3')
    m.obj('chest', x=23, y=12, flag='wr_chest1', items='keycard_blue:1,cell:20')
    m.obj('chest', x=36, y=22, flag='wr_chest2', items='stimpak:2,emp_grenade:2', gold=50)
    m.obj('sign', x=26, y=27, text='HULL BREACH 7 - "ARGENT DAWN" - colony vessel - (the rest is scorched away)')
    m.obj('savepoint', x=28, y=18)
    m.spawn_fix(27, 34)
    m.write(os.path.join(OUT, 'wreck.txt'), dict(id='wreck', name='The Wreck - Upper Hull', kind='ruins', spawn='27,34', bg='25,22,30'), leg)

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    brasswick(); millbrook(); hermit(); murkwater(); sunscar(); frosthold(); ashfall(); wreck()
