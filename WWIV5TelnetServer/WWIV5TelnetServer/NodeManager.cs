using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

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

    public NodeManager(int l, int h)
    {
      lowNode_ = l;
      highNode_ = h;
      var size = highNode_ - lowNode_ + 1;
      nodes_ = new List<NodeStatus>(size);
      for (int i = 0; i < size; i++)
      {
        nodes_.Add(new NodeStatus(i + lowNode_));
      }
    }

    /**
     * Gets the next free node or null of none exists.
     */
    public NodeStatus getNextNode()
    {
      lock (nodeLock)
      {
        foreach (NodeStatus node in nodes_)
        {
          if (!node.InUse)
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
