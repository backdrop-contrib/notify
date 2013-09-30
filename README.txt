------------------------
NOTIFY 7.x MODULE README
------------------------

This is a simple notification module. It provides e-mail notifications to
members about updates and changes to the Drupal web site.

Send comments via the issues queue on drupal.org:
http://drupal.org/project/issues/notify

------------------------
REQUIREMENTS
------------------------

This module requires a supported version of Drupal and cron to be
running.

------------------------
INSTALLATION
------------------------

1. Extract the notify project directory into the directory where you
   keep contributed modules (e.g. sites/all/modules/).

2. Enable the notify module on the Modules list page.  The database
   tables will be created automagically for you at this point.

3. Modify permissions on the People >> Permissions page.

   To set the notification checkbox default on new user registration
   form, or let new users opt in for notifications during
   registration, you must grant the anonymous user the right to access
   notify.

4. Configure general notification settings.  See the "Usage" section
   below for details.

------------------------
USAGE
------------------------

The administrative interface is at: Administer >> Configuration >>
People >> Notification settings.  The Settings tab is for setting how
often notifications are sent, and for selecting notification by note
type.  The Users tab is to review and see per-user settings.

When setting how often notifications are sent, note that e-mail
updates can only happen as frequently as the cron is set to run.
Check your cron settings.

If you enable node revisions (http://drupal.org/node/320614), the
notification e-mail will also mention the name of the last person to
edit the node.

To manage your own notification preferences, click on the
"Notification settings" on your "My account" page.

------------------------
AUTHOR / MAINTAINER
------------------------

Kjartan Mannes <kjartan@drop.org> is the original author.

Rob Barreca <rob@electronicinsight.com> was a previous maintainer.

Matt Chapman <matt@ninjitsuweb.com> is the current maintainer.

Ishmael Sanchez (http://ishmaelsanchez.com),
Ward Poelmans <wpoely86@gmail.com>,
John Oltman <john.oltman@sitebasin.com>,
Ajit Shinde (https://www.facebook.com/shinde.ajit), and 
Gisle Hannemyr <gisle@hannemyr.no> contributed to the Drupal 7 port.

------------------------
RELATED PROJECTS & ALTERNATIVES
------------------------

http://drupal.org/project/notify_by_views
http://drupal.org/project/subscriptions
http://drupal.org/project/notifications
