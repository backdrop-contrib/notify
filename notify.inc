<?php
// $Id$
function _notify_page($op, $edit) {
  global $user;
  if (user_access('access notify') && $user) {
    switch ($op) {
      case 'cron':
        _notify_send();
        break;
      case t('Save settings'):
        db_query('REPLACE {notify} (uid, status, node, teasers, comment) VALUES (%d, %d, %d, %d, %d)', $user->uid, $edit['status'], $edit['node'], $edit['teasers'], $edit['comment']);
        drupal_set_message(t('Notify settings saved'));
      default:
        $result = db_query('SELECT u.name, u.mail, n.status, n.node, n.teasers, n.comment FROM {users} u LEFT JOIN {notify} n ON u.uid = n.uid WHERE u.uid = %d AND u.status = 1 ORDER BY u.name', $user->uid);
        $notify = db_fetch_object($result);

        $group = form_radios(t('Notify status'), 'status', $notify->status, array(t('Disabled'), t('Enabled')), t('Do you wish to receive periodic emails when new content is posted?'));
        $form .= form_group(t('Master switch'), $group);
        $group = form_radios(t('Notify new content'), 'node', $notify->node, array(t('Disabled'), t('Enabled')), t('Include new content in the notification mail.'));
        $group .= form_radios(t('Content'), 'teasers', $notify->teasers, array(t('Title only'), t('Title + Teaser'), t('Title + Body')), t('Select the amount of each post that you would like to see in your notification emails.'));
        $group .= form_radios(t('Notify new comments'), 'comment', $notify->comment, array(t('Disabled'), t('Enabled')), t('Include new comments in the notification mail.'));
        $form .= form_group(t('Detailed settings'), $group);
        $form .= form_submit(t('Save settings'));

        print theme('page', form($form));
    }
  }
}

function _notify_admin($op, $edit) {
  switch ($op) {
    case t('Save'):
      foreach ($edit as $uid=>$settings) {
        db_query('UPDATE {notify} SET node = %d, teasers = %d, comment = %d WHERE uid = %d', $settings['node'], $settings['teasers'], $settings['comment'], $uid);
      }
    default:
      $result = db_query('SELECT u.uid, u.name, u.mail, n.* FROM {users} u LEFT JOIN {notify} n ON u.uid = n.uid WHERE n.status = 1 AND u.status = 1 ORDER BY u.name');
      $header = array (t('username'), t('email address'), t('content'), t('teasers'), t('comment'), t('failed attempts'));
      $i = 0;
      while ($notify = db_fetch_object($result)) {
        $rows[$i][] = format_name($notify);
        $rows[$i][] = $notify->mail;
        // 030312AX TODO: it really is a shame that we cannot use form_* functions
        // here (as they wrap everything into <p>title description</p>). implement
        // this once (see mailing list)!
        //$rows[$i][] = form_hidden("$notify->uid][status", 0) ."<input type=\"checkbox\" name=\"edit[$notify->uid][status]\"". ($notify->status ? " checked=\"checked\"" : "") .">";
        $rows[$i][] = form_hidden("$notify->uid][node", 0) .'<input type="checkbox" name="edit['. $notify->uid .'][node]" value="1"'. ($notify->node ? ' checked="checked"' : '') .'>';
        $select = '';
        foreach (array(t('Title only'), t('Title + Teaser'), t('Title + Body')) as $key => $choice) {
          $select .= '<option value="'. $key .'"'. ($key == $notify->teasers ? " selected=\"selected\"" : "") .">$choice</option>";
        }
        $rows[$i][] = '<select name="edit['. $notify->uid .'][teasers]">'. $select .'</select>';
        $rows[$i][] = form_hidden("$notify->uid][comment", 0) .'<input type="checkbox" name="edit['. $notify->uid .'][comment]" value="1"'. ($notify->comment ? ' checked="checked"' : '') .">";
        $rows[$i][] = $notify->attempts;
        $i++;
      }
      $output = theme('table', $header, $rows);
      $output.= form_submit(t('Save'));
      return $output;
  }
}

/**
 * Formatting of outgoing mail, taken from mail.inc, part of project.module
 */
function _notify_content($node, $notify) {
  static $i = 0;

  switch ($notify->teasers) {
    case 0:
      return;
    case 1:
      $txt = check_output($node->teaser);
      break;
    case 2:
      $txt = check_output($node->body);
  }

  $pattern = '@(<a href="(.+?)">(.+?)</a>)@ei';
  $txt = preg_replace($pattern, "'\\3 ['. _notify_mail_urls('\\2') .']'", $txt);
  $urls = _notify_mail_urls();
  if (count($urls)) {
      $txt .= "\n";
      for ($max = count($urls); $i < $max; $i++) {
        $txt .= '['. ($i + 1) .'] '. $urls[$i] ."\n";
      }
  }
  $txt = strip_tags($txt);
  $txt = entities_to_utf8($txt);
  return wordwrap($txt, 72);
}

function _notify_send() {
  global $base_url;
  $period = variable_get('notify_send_last', time() - variable_get('notify_send', 86400));

  // Fetch users with notify enabled
  $uresult = db_query("SELECT u.uid, u.name, u.mail, n.status, n.node, n.teasers, n.comment FROM {notify} n INNER JOIN {users} u ON n.uid = u.uid WHERE n.status = 1 AND u.status = 1 ". (($notify_attempts = variable_get('notify_attempts', 5)) ? ' AND n.attempts <= %d' : ''), $notify_attempts);
  while ($user = db_fetch_object($uresult)) {
    // Fetch all new nodes
    $nresult = db_query('SELECT DISTINCT(n.nid), n.body, n.type, n.title, n.promote, n.moderate, n.teaser, n.created, n.changed, u.name FROM {node} n '. node_access_join_sql() .' INNER JOIN {users} u ON n.uid = u.uid WHERE n.status = 1 AND n.created > %d AND '. node_access_where_sql('view', 'na', $user->uid) .' ORDER BY n.created', $period);
    $nodes = array();
    while ($node = db_fetch_object($nresult)) {
      $nodes[$node->nid] = $node;
    }

    // Fetch new comments
    $cresult = db_query('SELECT DISTINCT(c.cid), c.nid, c.subject, c.pid, u.name FROM {comments} c '. node_access_join_sql('c') .' INNER JOIN {users} u ON u.uid = c.uid WHERE c.status = 0 AND c.timestamp > %d AND '. node_access_where_sql('view', 'na', $user->uid) .' ORDER BY c.nid, c.timestamp', $period);
    $comments = array();
    while ($comment = db_fetch_object($cresult)) {
      $comments[$comment->nid][] = $comment;
    }

    $from = variable_get('site_mail', ini_get('sendmail_from'));
    $subject = t('%sitename notification for %username', array('%username' => $user->name, '%sitename' => variable_get('site_name', 'drupal')));
    $body = '';
    // New content
    if ($user->node && count($nodes)) {
      $body .= t('Recent content') ."\n". str_repeat('-', 78) ."\n\n";
      foreach ($nodes as $node) {
        if ($user->moderate) {
          $body .= strtr(t('%status %type by %author: %title'), array('%status' => t('queued'), '%type' => node_invoke($node, 'node_name'), '%title' => $node->title, '%author' => ($node->name ? $node->name : variable_get('anonymous', 'Anonymous')))) ."\n";
          $body .= _notify_content($node, $user);
          $body .= '  [ '. url("queue/$node->nid", NULL, NULL, TRUE) ." ]\n\n";
        }
        elseif (!$user->moderate && $node->nid) {
          $body .= strtr(t('%status %type by %author: %title'), array('%status' => t('published'), '%type' => node_invoke($node, 'node_name'), '%title' => $node->title, '%author' => ($node->name ? $node->name : variable_get('anonymous', 'Anonymous')))) ."\n";
          $body .= _notify_content($node, $user);
          $body .= '  [ '. url("node/$node->nid", NULL, NULL, TRUE) ." ]\n\n";
        }
      }
    }

    // New comments
    if ($user->comment && count($comments)) {
      $body .= t('Recent comments') ."\n". str_repeat('-', 78) ."\n\n";
      foreach ($comments as $nid => $comment) {
        if ($nid != $nid_old) {
          if (strlen($nodes[$nid]->title) < 2) {
            $result = db_query("SELECT title FROM {node} WHERE nid = %d", $nid);
            $nodes[$nid] = db_fetch_object($result);
          }
          $body .= strtr(t('%count new comments attached to: %title'), array('%count' => count($comment), '%title' => $nodes[$nid]->title)) ."\n";
          $nid_old = $nid;
        }
        foreach ($comment as $c) {
          $body .= '  '. strtr(t('%title by %author'), array('%title' => $c->subject, '%author' => ($c->name ? $c->name : variable_get(anonymous, 'Anonymous')))) ."\n"
                .  '    '. url("node/$nid/$c->cid#$c->cid", NULL, NULL, TRUE) ."\n\n";
        }
      }
    }
    if ($body) {
      $body = t('Greetings %user,', array("%user" => $user->name))."\n\n$body";

      $body .= "\n-- \n";
      $body .= t('This is an automatic mail from %sitename', array('%sitename' => variable_get('site_name', 'drupal')))."\n";
      $body .= t('To stop receiving these mails go to %notify-url', array('%notify-url' => url("user/$user->uid/notify" , NULL, NULL, TRUE)))."\n";

      if (!user_mail($user->mail, $subject, wordwrap($body, 72), "From: $from\nReply-to: $from\nX-Mailer: Drupal\nReturn-path: <$from>\nErrors-to: $from\n")) {
        db_query('UPDATE {notify} SET attempts = attempts + 1 WHERE uid = %d', $user->uid);
        watchdog('error', t('Notify: User %name (%mail) could not be notified. Mail error.', array('%name' => '<em>'. $user->name .'</em>', '%mail' => $user->mail)));
      }
      else {
        watchdog('user', t('Notify: User %name (%mail) notified successfully.', array('%name' => '<em>'. $user->name .'</em>', '%mail' => $user->mail)));
      }
    }
  }
}

function entities_to_utf8($text) {
  static $table;
  // We store named entities in a table for quick processing.  
  if (!isset($table)) {
    // Get all named HTML entities.
    $table = array_flip(get_html_translation_table(HTML_ENTITIES, $special));
    // PHP gives us Windows-1252/ISO-8859-1 data, we need UTF-8.
    $table = array_map('utf8_encode', $table);
  }
  $text = strtr($text, $table);

  // Any remaining entities are numerical. Use a regexp to replace them.
  return preg_replace('/&#(x?)([A-Za-z0-9]+);/e', '_entity_to_utf8("$1", "$2")', $text);
}

function _entity_to_utf8($hex, $codepoint) {
  if ($hex != '') {
    $codepoint = base_convert($codepoint, 16, 10);
  }
  if ($codepoint < 0x80) {
    return chr($codepoint);
  }
  else if ($codepoint < 0x800) {
    return chr(0xC0 | ($codepoint >> 6))
         . chr(0x80 | ($codepoint & 0x3F));
  }
  else if ($codepoint < 0x10000) {
    return chr(0xE0 | ( $codepoint >> 12))
         . chr(0x80 | (($codepoint >> 6) & 0x3F))
         . chr(0x80 | ( $codepoint       & 0x3F));
  }
  else if ($codepoint < 0x200000) {
    return chr(0xF0 | ( $codepoint >> 18))
         . chr(0x80 | (($codepoint >> 12) & 0x3F))
         . chr(0x80 | (($codepoint >> 6)  & 0x3F))
         . chr(0x80 | ( $codepoint        & 0x3F));
  }
}

function _notify_mail_urls($url = 0) {
  static $urls = array();
  if ($url) {
    $urls[] = strpos($url, '://') ? $url : url($url, NULL, NULL, 1);
    return count($urls);
  }
  return $urls;
}
?>
