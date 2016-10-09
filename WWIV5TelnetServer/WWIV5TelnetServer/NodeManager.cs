using System;
using System.Collections.Generic;

namespace WWIV5TelnetServer
{
  class NodeManager
  {
    private Object nodeLock = new Object();

    private int lowNode_;
    private int highNode_;
    private List<NodeStatus> nodes_;

    // Property for Nodes
    public List<NodeStatus> Nodes { get { return nodes_; } }

    public NodeManager(int l, int h, bool binkp)
    {
      lowNode_ = l;
      highNode_ = h;
      var size = highNode_ - lowNode_ + 1;
      nodes_ = new List<NodeStatus>(size);
      for (int i = 0; i < size; i++)
      {
        nodes_.Add(new NodeStatus(NodeType.BBS, i + lowNode_));
      }
      if (binkp)
      {
        nodes_.Add(new WWIV5TelnetServer.NodeStatus(NodeType.BINKP, 0));
      }
    }

    /**
     * Gets the next free node or null of none exists.
     */
    public NodeStatus getNextNode(NodeType type)
    {
      lock (nodeLock)
      {
        foreach (NodeStatus node in nodes_)
        {
          if (!node.InUse && node.NodeType == type)
          {
            // Mark it in use.
            node.InUse = true;
            // return it.
            return node;
          }
        }
      }
      // No node is available, return null.
      return null;
    }

    public void freeNode(NodeStatus node)
    {
      lock (nodeLock)
      {
        node.InUse = false;
      }
    }
  }
}