package app.organicmaps.traffxml;

import android.content.BroadcastReceiver;
import android.content.Context;

/**
 * Abstract superclass for TraFF source implementations.
 */
public abstract class SourceImpl extends BroadcastReceiver
{
  /**
   * Creates a new instance.
   * 
   * @param context The application context
   */
  public SourceImpl(Context context, long nativeManager)
  {
    super();
    this.context = context;
    this.nativeManager = nativeManager;
  }
  
  protected Context context;
  
  /**
   * The native `TraffSourceManager` instance.
   */
  protected long nativeManager;
  
  /**
   * Subscribes to a traffic source.
   * 
   * @param filterList The filter list in XML format
   */
  public abstract void subscribe(String filterList);
  
  /**
   * Changes an existing traffic subscription.
   * 
   * @param filterList The filter list in XML format
   */
  public abstract void changeSubscription(String filterList);
  
  /**
   * Unsubscribes from a traffic source we are subscribed to.
   */
  public abstract void unsubscribe();
  
  /**
   * Forwards a newly received TraFF feed to the traffic module for processing.
   *
   * Called when a TraFF feed is received. This is a wrapper around {@link #onFeedReceivedImpl(long, String)}.
   *
   * @param feed The TraFF feed
   */
  protected void onFeedReceived(String feed)
  {
    onFeedReceivedImpl(nativeManager, feed);
  }
  
  /**
   * Forwards a newly received TraFF feed to the traffic module for processing.
   *
   * Called when a TraFF feed is received.
   *
   * @param nativeManager The native `TraffSourceManager` instance
   * @param feed The TraFF feed
   */
  protected static native void onFeedReceivedImpl(long nativeManager, String feed);
}
