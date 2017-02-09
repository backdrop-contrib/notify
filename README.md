# Notify

This is a port to Backdrop of the Drupal project version 7.x-1.3.

Notify module sends e-mail digests of new content and comments.

This is a lightweight module that allows users to subscribe to periodic 
emails which include all new or revised content and/or comments of 
specific content types, much like the daily newsletters sent by some websites.

Even if this feature is not configured for normal site users, it can be
 a useful feature for an administrator of a site to receive notification 
 of new content submissions and comment posts.

The notifications are sent out in batches (digest style) at a frequency 
determined by the administrator. The administrator also decides what 
content type(s) to make available for notification subscriptions.

Ordinary users can turn notifications on and off, whether to receive 
just headlines, teasers, or complete nodes/comments, 
and what content types to subscribe to.

## Status

This port to Backdrop is not yet fully ported and tested.


## Installation

Install this module using the official Backdrop CMS instructions at
https://backdropcms.org/guide/modules

Notify includes one submodule Notify Views Integration.
 
## Use 

Once installed the configuration and other administrative settings are available
at admin/config/people/notify, i.e. under 'User accounts' in the Configuration 
menu.

The submodule Notify Views Integration tells the Views module 
about the Notify database so that listings of current per user
settings can be created.

When adding such a view choose 'Show User accounts'. Then when adding
fields to the view six types of notify field are available:
+ How much to notify about : Title only; Title + Teaser; etc.
+ Master switch status : 1 = enabled
+ New comments : 1 = enabled
+ New content : 1 = enabled
+ Number of failed attempts for the user.
+ UID : The ID of the user

## Tests

The module includes some tests - see file notify.test.
These have not yet been ported to Backdrop.

## License

This project is GPL v2 software.
See the LICENSE.txt file for complete text.

## Current Maintainers

### For Drupal:
+ Gisle Hannemyr (gisle)
+ Matt Chapman (matt2000)

### Port to Backdrop:

  - Graham Oliver github.com/Graham-72


## Acknowledgement

This port to Backdrop would not, of course, be possible without
all the work done by the developers and maintainers of the Drupal module.