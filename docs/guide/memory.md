# Memory

Nodes live in one growing vector inside an `Arena` and are addressed by index,
not by pointer. Children are an intrusive first-child/next-sibling list. Text is
interned into fixed blocks and handed out as `string_view`.

This page explains why, because the design shows up in every API you touch.

## Why not `shared_ptr`

A UI tree is built often — whenever state changes — walked twice per frame, and
thrown away whole. That access pattern rewards contiguous storage and punishes
per-node allocation.

| | Arena + index | `shared_ptr` tree |
| --- | --- | --- |
| Creating a node | `push_back` | an allocation |
| Walking children | sequential memory | pointer chasing |
| Releasing a tree | one `reset()` | a destructor cascade |
| Cycles | impossible | a leak waiting to happen |
| After a reallocation | a `NodeId` still works | a `Node*` dangles |

A `Node` is 560 bytes — style, text style, icon content, the frame and the
content box, all inline — so a screen of a few hundred nodes is a couple of
hundred kilobytes, allocated once and rewritten every frame. The panel example
reports its own: 111 nodes and 80 draw commands, out of an arena reserved at 256
nodes and reused for the life of the process.

## The rules that follow

**A `NodeId` is an index, not a pointer.** It survives the arena growing. It
does *not* survive `reset()`, so nothing may hold one across frames — use a
[tag](/guide/building-a-tree#naming-a-node) for identity instead.

**A reference into the arena is only valid until the next `create`.** This is
the one sharp edge, and it drew blood once: layout held a reference into its
scratch vector, recursed into a child, and the recursion grew the vector under
it. Frames came out at y = 5.8e25. If you take a reference and then call
anything that can add a node, copy instead.

```cpp
const Item item = scratch_[index];   // copy: place() below can grow the vector
place(item.id, frame);
cursor += item.target;
```

**Interned text is stable.** `arena.intern()` copies into fixed 4 KiB blocks
that are never reallocated — a `std::deque` of arrays, not a `vector<char>` —
because a view into a buffer that later grows is a dangling read.

**Vector art is owned the same way.** `arena.addShapes()` takes a vector of
`Shape` and returns where it landed; a node names a *range* into that store
rather than holding a container of its own, which is what keeps `Node` trivial
to clear. A chart is therefore one node and a slice of geometry, not a subtree.

## Reserving

```cpp
Arena arena;
arena.reserve(512);      // one allocation for the whole frame
```

Building then allocates nothing at all. `reset()` keeps the capacity, so the
next frame writes over the same pages:

```cpp
while (running) {
    arena.reset();       // O(1): the previous frame is simply forgotten
    Ui ui(arena);
    // …
}
```

## Measuring

```cpp
std::printf("%zu nodes, %zu bytes\n", arena.size(), arena.bytesUsed());
```

`bytesUsed` counts the node vector's capacity and every string block, which is
everything the tree owns.
