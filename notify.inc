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

        $form .= form_select(t('Notify status'), 'status', $notify->status, array(t('Disabled'), t('Enabled')), t('Do you wish to receive periodic emails when new content is posted?'));
        $form .= form_select(t('Notify new content'), 'node', $notify->node, array(t('Disabled'), t('Enabled')), t('Include new content in the notification mail.'));
        $form .= form_select(t('Content'), 'teasers', $notify->teasers, array(t('Title only'), t('Title + Teaser'), t('Title + Body')), t('Select the amount of each post that you would like to see in your notification emails.'));
        $form .= form_select(t('Notify new comments'), 'comment', $notify->comment, array(t('Disabled'), t('Enabled')), t('Include new comments in the notification mail.'));
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

function _notify_content($node, $notify) {
  switch ($notify->teasers) {
    case 0:
      return;
    case 1:
      $txt = check_output($node->teaser);
      break;
    case 2:
      $txt = check_output($node->body);
  }
  $txt = ereg_replace ("(<br />|<br/>|<p>)", _notify_lb(), $txt);
  $txt = html_entity_decode($txt);
  $txt = strip_tags($txt);
  return $txt;
}

function _notify_lb() {
  $os = php_uname();
  switch ($os) {
    case strstr($os, 'Windows'):
      return "\r\n";
    case strstr($os, 'nix'):
      return "\n";
    case strstr($os, 'Mac'):
      return "\r";
    default:
      return "\n";
  }
}

function _notify_send() {
  global $base_url;
  $period = variable_get('notify_send_last', time() - variable_get('notify_send', 86400));

  // Fetch users with notify enabled
  $uresult = db_query("SELECT u.uid, u.name, u.mail, n.status, n.node, n.teasers, n.comment FROM {notify} n INNER JOIN {users} u ON n.uid = u.uid WHERE n.status = 1 AND u.status = 1 ". (($notify_attempts = variable_get('notify_attempts', 5)) ? ' AND n.attempts <= %d' : ''), $notify_attempts);
  while ($user = db_fetch_object($uresult)) {
    // Fetch all new nodes
    $nresult = db_query('SELECT n.body, n.nid, n.type, n.title, n.promote, n.moderate, n.teaser, n.created, n.changed, u.name FROM {node} n '. node_access_join_sql() .' INNER JOIN {users} u ON n.uid = u.uid WHERE n.status = 1 AND n.created > %d AND '. node_access_where_sql('view', 'na', $user->uid) .' ORDER BY n.created', $period);
    $nodes = array();
    while ($node = db_fetch_object($nresult)) {
      $nodes[$node->nid] = $node;
    }

    // Fetch new comments
    $cresult = db_query('SELECT c.cid, c.nid, c.subject, c.pid, u.name FROM {comments} c INNER JOIN {node} n ON n.nid = c.nid '. node_access_join_sql() .' INNER JOIN {users} u ON u.uid = c.uid WHERE c.status = 0 AND c.timestamp > %d AND '. node_access_where_sql('view', 'na', $user->uid) .' ORDER BY c.nid, c.timestamp', $period);
    $comments = array();
    while ($comment = db_fetch_object($cresult)) {
      $comments[$comment->nid][] = $comment;
    }

    $from = variable_get('site_mail', ini_get('sendmail_from'));
    $subject = variable_get('site_name', 'drupal') .' '. t('notification for %name', array ('%name' =>$user->name));
    $body = '';
    // New content
    if ($user->node && count($nodes)) {
      $body .= t('Recent content') ."\n". str_repeat('-', strlen(t('Recent content'))) ."\n\n";
      foreach ($nodes as $node) {
        if ($user->moderate) {
          $body .= strtr(t('%status %type by %author: %title'), array('%status' => 'queued', '%type' => node_invoke($node, 'node_name'), '%title' => $node->title, '%author' => ($node->name ? $node->name : variable_get('anonymous', 'Anonymous')))) ."\n";
          $body .= _notify_content($node, $user);
          $body .= '  [ '. url("queue/$node->nid", NULL, NULL, TRUE) ." ]\n\n";
        }
        elseif (!$user->moderate && $node->nid) {
          $body .= strtr(t('%status %type by %author: %title'), array('%status' => 'published', '%type' => node_invoke($node, 'node_name'), '%title' => $node->title, '%author' => ($node->name ? $node->name : variable_get('anonymous', 'Anonymous')))) ."\n";
          $body .= _notify_content($node, $user);
          $body .= '  [ '. url("node/$node->nid", NULL, NULL, TRUE) ." ]\n\n";
        }
      }
    }

    // New comments
    if ($user->comment && count($comments)) {
      $body .= t('Recent comments') ."\n". str_repeat('-', strlen(t('Recent comments'))) ."\n\n";
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
      $body = t('Greetings') .' '. $user->name .",\n\n$body";

      $body .= "\n-- \n";
      $body .= t('This is an automatic mail from') .' '. variable_get('site_name', 'drupal') ."\n";
      $body .= t('To stop receiving these mails go to') .' '. url("user/$user->uid/notify" , NULL, NULL, TRUE) ."\n";

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
?>
