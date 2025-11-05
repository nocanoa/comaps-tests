package app.organicmaps.sdk.bookmarks.data;

/**
 * represents the details of the socket available on a particular charging station
 *
 */
public record ChargeSocketDescriptor(String type, int count, double power) {}
