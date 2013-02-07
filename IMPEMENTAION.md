## Implementation musings.

This module dynamically creates, destroys, and configures torus nodes through
rtnetlink.  Each node becomes the master of the four port interfaces connected
to adjacent nodes.  The node may also be master of up to two other interfaces
peering with remote toroids or up-link carriers.  One may also clone these
nodes for virtual hosting.

FIXME with the rest.
