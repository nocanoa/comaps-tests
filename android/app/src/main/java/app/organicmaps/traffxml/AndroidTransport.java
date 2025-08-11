/*
 * Copyright © 2019–2020 traffxml.org.
 * 
 * Relicensed to CoMaps by the original author.
 */

package app.organicmaps.traffxml;

public class AndroidTransport {
    /**
     * Intent to poll a peer for its capabilities.
     * 
     * <p>This is a broadcast intent and must be sent as an explicit broadcast.
     */
    public static final String ACTION_TRAFF_GET_CAPABILITIES = "org.traffxml.traff.GET_CAPABILITIES";

    /**
     * Intent to send a heartbeat to a peer.
     * 
     * <p>This is a broadcast intent and must be sent as an explicit broadcast.
     */
    public static final String ACTION_TRAFF_HEARTBEAT = "org.traffxml.traff.GET_HEARTBEAT";

    /**
     * Intent to poll a source for information.
     * 
     * <p>This is a broadcast intent and must be sent as an explicit broadcast.
     * 
     * <p>Polling is a legacy feature on Android and deprecated in TraFF 0.8 (rather than polling, TraFF 0.8
     * applications query the content provider). Therefore, poll operations are subscriptionless, and the
     * source should either reply with all messages it currently holds, or ignore the request.
     */
    @Deprecated
    public static final String ACTION_TRAFF_POLL = "org.traffxml.traff.POLL";

    /**
     * Intent for a push feed.
     * 
     * <p>This is a broadcast intent. It can be used in different forms:
     * 
     * <p>As of TraFF 0.8, it must be sent as an explicit broadcast and include the
     * {@link #EXTRA_SUBSCRIPTION_ID} extra. The intent data must be a URI to the content provider from which
     * the messages can be retrieved. The {@link #EXTRA_FEED} extra is not supported. The feed is part of a
     * subscription and will contain only changes over feeds sent previously as part of the same
     * subscription.
     * 
     * <p>Legacy applications omit the {@link #EXTRA_SUBSCRIPTION_ID} extra and may send it as an implicit
     * broadcast. If an application supports both legacy transport and TraFF 0.8 or later, it must include
     * the {@link #EXTRA_PACKAGE} extra. The feed is sent in the {@link #EXTRA_FEED} extra, as legacy
     * applications may not support content providers. If sent as a response to a subscriptionless poll, the
     * source should include all messages it holds, else the set of messages included is at the discretion of
     * the source.
     * 
     * <p>Future applications may reintroduce unsolicited push operations for certain scenarios.
     */
    public static final String ACTION_TRAFF_PUSH = "org.traffxml.traff.FEED";

    /**
     * Intent for a subscription request.
     * 
     * <p>This is a broadcast intent and must be sent as an explicit broadcast.
     * 
     * <p>The filter list must be specified in the {@link #EXTRA_FILTER_LIST} extra.
     * 
     * <p>The sender must indicate its package name in the {@link #EXTRA_PACKAGE} extra.
     */
    public static final String ACTION_TRAFF_SUBSCRIBE = "org.traffxml.traff.SUBSCRIBE";

    /**
     * Intent for a subscription change request,
     * 
     * <p>This is a broadcast intent and must be sent as an explicit broadcast.
     * 
     * <p>This intent must have {@link #EXTRA_SUBSCRIPTION_ID} set to the ID of an existing subscription between
     * the calling consumer and the source which receives the broadcast.
     * 
     * <p>The new filter list must be specified in the {@link #EXTRA_FILTER_LIST} extra.
     */
    public static final String ACTION_TRAFF_SUBSCRIPTION_CHANGE = "org.traffxml.traff.SUBSCRIPTION_CHANGE";

    /**
     * Intent for an unsubscribe request,
     * 
     * <p>This is a broadcast intent and must be sent as an explicit broadcast.
     * 
     * <p>This intent must have {@link #EXTRA_SUBSCRIPTION_ID} set to the ID of an existing subscription between
     * the calling consumer and the source which receives the broadcast. It signals that the consumer is no
     * longer interested in receiving messages related to that subscription, and that the source should stop
     * sending updates. Unsubscribing from a nonexistent subscription is a no-op.
     */
    public static final String ACTION_TRAFF_UNSUBSCRIBE = "org.traffxml.traff.UNSUBSCRIBE";

    /**
     * Name for the column which holds the message data.
     */
    public static final String COLUMN_DATA = "data";

    /**
     * Schema for TraFF content URIs.
     */
    public static final String CONTENT_SCHEMA = "content";
    
    /**
     * String representations of TraFF result codes
     */
    public static final String[] ERROR_STRINGS = {
        "unknown (0)",
        "invalid request (1)",
        "subscription rejected by the source (2)",
        "requested area not covered (3)",
        "requested area partially covered (4)",
        "subscription ID not recognized by the source (5)",
        "unknown (6)",
        "source reported an internal error (7)"
    };

    /**
     * Extra which contains the capabilities of the peer.
     * 
     * <p>This is a String extra. It contains a {@code capabilities} XML element.
     */
    public static final String EXTRA_CAPABILITIES = "capabilities";

   /**
     * Extra which contains a TraFF feed.
     * 
     * <p>This is a String extra. It contains a {@code feed} XML element.
     * 
     * <p>The sender should be careful to keep the size of this extra low, as Android has a 1 MByte limit on all
     * pending Binder transactions. However, there is no feedback to the sender about the capacity still
     * available, or whether a request exceeds that limit. Therefore, senders should keep the size if each
     * feed significantly below that limit. If necessary, they should split up a feed into multiple smaller
     * ones and send them with a delay in between.
     * 
     * <p>This mechanism is deprecated since TraFF 0.8 and peers are no longer required to support it. Peers
     * which support TraFF 0.8 must rely on content providers for message transport.
     */
    @Deprecated
    public static final String EXTRA_FEED = "feed";

    /**
     * Extra which contains a filter list.
     * 
     * <p>This is a String extra. It contains a {@code filter_list} XML element.
     */
    public static final String EXTRA_FILTER_LIST = "filter_list";

    /**
     * Extra which contains the package name of the app sending it.
     * 
     * <p>This is a String extra.
     */
    public static final String EXTRA_PACKAGE = "package";

    /**
     * Extra which contains a subscription ID.
     * 
     * <p>This is a String extra.
     */
    public static final String EXTRA_SUBSCRIPTION_ID = "subscription_id";

    /**
     * Extra which contains the timeout duration for a subscription.
     * 
     * <p>This is an integer extra.
     */
    public static final String EXTRA_TIMEOUT = "timeout";

    /**
     * The MIME type for TraFF content providers.
     */
    public static final String MIME_TYPE_TRAFF = "vnd.android.cursor.dir/org.traffxml.message";

    /**
     * The operation completed successfully.
     */
    public static final int RESULT_OK = -1;

    /**
     * An internal error prevented the recipient from fulfilling the request.
     */
    public static final int RESULT_INTERNAL_ERROR = 7;

    /**
     * A nonexistent operation was attempted, or an operation was attempted with incomplete or otherwise
     * invalid data.
     */
    public static final int RESULT_INVALID = 1;

    /**
     * The subscription was rejected, and no messages will be sent.
     */
    public static final int RESULT_SUBSCRIPTION_REJECTED = 2;

    /**
     * The subscription was rejected because the source will never provide messages matching the selection.
     */
    public static final int RESULT_NOT_COVERED = 3;

    /**
     * The subscription was accepted but the source can only provide messages for parts of the selection.
     */
    public static final int RESULT_PARTIALLY_COVERED = 4;

    /**
     * The request failed because it refers to a subscription which does not exist between the source and
     * consumer involved.
     */
    public static final int RESULT_SUBSCRIPTION_UNKNOWN = 5;

    /**
     * The request failed because the aggregator does not accept unsolicited push requests from the sensor.
     */
    public static final int RESULT_PUSH_REJECTED = 6;
    
    public static String formatTraffError(int code) {
      if ((code < 0) || (code >= ERROR_STRINGS.length))
        return String.format("unknown (%d)", code);
      else
        return ERROR_STRINGS[code];
    }
}
