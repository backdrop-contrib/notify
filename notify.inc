<?php
// $Id$

function _notify_page($uid) {
  global $user;
  
  $account = user_load(array('uid' => $uid));
  if ($account === FALSE) {
    return drupal_not_found();
  }
  
  $op = $_POST['op'];
  $edit = $_POST['edit'];
  
  // If user is logged in and either has admin privileges or is the editing account
  if ($user && (user_access('administer notify') || (user_access('access notify') && $user->uid == $account->uid))) {
    switch ($op) {
      case 'cron':
        _notify_send();
        break;
      default:
        $result = db_query('SELECT u.uid, u.name, u.mail, n.status, n.node, n.teasers, n.comment FROM {users} u LEFT JOIN {notify} n ON u.uid = n.uid WHERE u.uid = %d AND u.status = 1 ORDER BY u.name', $account->uid);
        $notify = db_fetch_object($result);

        if ($notify->mail) {
          $form['notify_page_master'] = array('#type' => 'fieldset','#title' => t('Master switch'));
          $form['notify_page_master']['status'] = array('#type' => 'radios', '#title' => t('Notify status'), '#default_value' => $notify->status, '#options' =>  array(t('Disabled'), t('Enabled')), '#description' => t('Do you wish to receive periodic emails when new content is posted?'));

          $form['notify_page_detailed'] = array('#type' => 'fieldset', '#title' => t('Detailed settings'));
          $form['notify_page_detailed']['node'] = array('#type' => 'radios', '#title' => t('Notify new content'), '#default_value' => $notify->node, '#options' => array(t('Disabled'), t('Enabled')), '#description' => t('Include new content in the notification mail.'));
          $form['notify_page_detailed']['teasers'] = array('#type' => 'radios', '#title' => t('Content'), '#default_value' => $notify->teasers, '#options' => array(t('Title only'), t('Title + Teaser'), t('Title + Body')), '#description' => t('Select the amount of each post that you would like to see in your notification emails.'));
          $form['notify_page_detailed']['comment'] = array('#type' => 'radios', '#title' => t('Notify new comments'), '#default_value' => $notify->comment, '#options' => array(t('Disabled'), t('Enabled')), '#description' => t('Include new comments in the notification mail.'));
          $form['submit'] = array('#type' => 'submit', '#value' => t('Save settings'));
        }
        else {
          drupal_set_message(t('Your e-mail address must be specified on your <a href="%url">my account</a> page.', array('%url' => url('user/'.$notify->uid.'/edit'))), 'error');
        }

        return drupal_get_form('notify_page', $form);
    }
  }
}

function notify_page_submit($form_id, $edit) {
  global $user;
  
  $account = user_load(array('uid' => arg(1)));
  if ($account === FALSE) {
    return drupal_not_found();
  }
  
  if ($user && (user_access('administer notify') || (user_access('access notify') && $user->uid == $account->uid))) {
    db_query('DELETE FROM {notify} WHERE uid = %d', $account->uid);
    db_query('INSERT INTO {notify} (uid, status, node, teasers, comment) VALUES (%d, %d, %d, %d, %d)', $account->uid, $edit['status'], $edit['node'], $edit['teasers'], $edit['comment']);
    drupal_set_message(t('Notify settings saved.'));
  }
}

function _notify_admin() {
  $op = $_POST['op'];
  $edit = $_POST['edit'];
  
  switch ($op) {
    case t('Save and send now'):
      $status = _notify_send();
      variable_set('notify_send_last', time());
      cache_clear_all('variables');
      
      if ($status['sent'] > 0) {
        drupal_set_message(t('%count pending notification e-mails have been sent.', array('%count' => $status['sent'])));
      }
      elseif ($status['failed'] > 0) {
        drupal_set_message(t('%count notification e-mails could not be sent.', array('%count' => $status['failed'])), 'error');
      }
      else {
        drupal_set_message(t('No notification e-mails needed to be sent.'));
      }
    
    case t('Save'):
      foreach ($edit['users'] as $uid => $settings) {
        db_query('UPDATE {notify} SET node = %d, teasers = %d, comment = %d WHERE uid = %d', 
          $settings['node'], $settings['teasers'], $settings['comment'], $uid);
      }
      drupal_set_message(t('Notify settings saved.'));
    
    default:
      $result = db_query('SELECT u.uid, u.name, u.mail, n.* FROM {users} u LEFT JOIN {notify} n ON u.uid = n.uid WHERE n.status = 1 AND u.status = 1 ORDER BY u.name LIMIT 5');

      $form = array();
      $form['notify_admin'] = array();
      $form['notify_admin']['#theme'] = 'notify_admin_default';
      $form['notify_admin']['info'] = array('#type' => 'markup', '#value' => '<em>'. t('The following table shows all users that have notifications enabled.') .'</em>');

      while ($notify = db_fetch_object($result)) {
        $form['notify_admin']['users'][$notify->uid]['name'] = array('#type' => 'markup', '#value' => theme('username', $notify));
        $form['notify_admin']['users'][$notify->uid]['mail'] = array('#type' => 'markup', '#value' => $notify->mail);
        $form['notify_admin']['users'][$notify->uid]['node'] = array('#type' => 'checkbox', '#default_value' => $notify->node);
        $form['notify_admin']['users'][$notify->uid]['teasers'] = array('#type' => 'select', '#default_value' => $notify->teasers, '#options' => array(t('Title only'), t('Title + Teaser'), t('Title + Body')));
        $form['notify_admin']['users'][$notify->uid]['comment'] = array('#type' => 'checkbox', '#default_value' => $notify->comment);
        $form['notify_admin']['users'][$notify->uid]['attempts'] = array('#type' => 'markup', '#value' => intval($notify->attempts));
      }
      $form['submit'] = array('#type' => 'submit', '#value' => t('Save'));
      $form['submitandsendnow'] = array('#type' => 'submit', '#value' => t('Save and send now'));
      return drupal_get_form('notify_admin', $form);
  }
}

function theme_notify_admin_default($form) {
  $output = form_render($form['info']);
  $header = array (t('username'), t('e-mail address'), t('content'), t('teasers'), t('comment'), t('failed attempts'));
  
  $rows = array();
  foreach (element_children($form['users']) as $uid) {
    $row = array();
    foreach ($form['users'][$uid] as $entry_key => $entry) {
      if (substr($entry_key, 0, 1) == '#') {
        continue;
      }
      unset($form['users'][$uid][$entry_key]['#title']);
       
      // Manually get the correct form field name. Is this the right way?
      $form['users'][$uid][$entry_key]['#name'] = 'edit[users]['.$uid.']['.$entry_key.']';
      
      $row[] = form_render($form['users'][$uid][$entry_key]);
    }
    $rows[] = $row;
  }
  $output .= theme('table', $header, $rows);

  $output .= form_render($form);
  return $output;
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
      $txt = check_markup($node->teaser, $node->format, false);
      break;
    case 2:
      $txt = check_markup($node->body, $node->format, false);
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
  $txt = notify_entities_to_utf8($txt);
  return wordwrap($txt, 72);
}

function _notify_send() {
  $period = variable_get('notify_send_last', time() - variable_get('notify_send', 86400));
  $separator = '------------------------------------------------------------------------------';
  $mini_separator = '---';
  
  $ret = array('sent' => 0, 'failed' => 0);
  
  _notify_switch_user(); // Store current user
  
  // Fetch users with notify enabled
  $uresult = db_query("SELECT u.uid, u.name, u.mail, n.status, n.node, n.teasers, n.comment FROM {notify} n INNER JOIN {users} u ON n.uid = u.uid WHERE n.status = 1 AND u.status = 1 ". (($notify_attempts = variable_get('notify_attempts', 5)) ? ' AND n.attempts <= %d' : ''), $notify_attempts);
  
  while ($user = db_fetch_object($uresult)) {
    _notify_switch_user($user->uid);
    
    // Fetch all new nodes
    $nresult = db_query(db_rewrite_sql('SELECT n.nid, r.body, n.type, n.title, n.promote, n.moderate, r.teaser, n.created, n.changed, u.name FROM {node} n INNER JOIN {node_revisions} r ON n.vid = r.vid INNER JOIN {users} u ON n.uid = u.uid WHERE (n.status = 1 OR n.moderate = 1) AND n.created > %d ORDER BY n.created'), $period);
    $nodes = array();
    while ($node = db_fetch_object($nresult)) {
      $nodes[$node->nid] = $node;
    }

    // Fetch new comments
    $cresult = db_query(db_rewrite_sql('SELECT c.nid, c.cid, c.subject, c.pid, u.name FROM {comments} c INNER JOIN {users} u ON u.uid = c.uid WHERE c.status = 0 AND c.timestamp > %d ORDER BY c.nid, c.timestamp', 'c'), $period);
    $comments = array();
    while ($comment = db_fetch_object($cresult)) {
      $comments[$comment->nid][] = $comment;
    }

    $from = variable_get('site_mail', ini_get('sendmail_from'));
    $from_name = mime_header_encode(variable_get('site_name', 'drupal'));
    $subject = t('%sitename new content notification for %username', array('%username' => $user->name, '%sitename' => variable_get('site_name', 'drupal')));
    $body = '';
    
    // Write new node content to e-mail
    if ($user->node && count($nodes)) {
      $body .= $separator . "\n"
        . t('Recent content - %count', array('%count' => format_plural(count($nodes), '1 new node', '%count new nodes'))) ."\n"
        . $separator . "\n\n";
      
      $node_count = 0;
      foreach ($nodes as $node) {
        $temp = node_invoke($node, 'node_info');
        $node->type_name = $temp[$node->type]['name'];
        $nodes[$node->nid] = $node;
        
        if ($node->moderate) {
          $status = t('Queued');
          $link = 'node';
        }
        else {
          $status = t('Published');
          $link = 'node';
        }
        
        $body .= ++$node_count.'. '.t('%title', array('%title' => $node->title)) ."\n";
        $body .= t('%status %type by %author', array('%status' => $status, '%type' => $node->type_name, '%author' => ($node->name ? $node->name : variable_get('anonymous', 'Anonymous')))) ."\n";
        $body .= '[ '. url($link .'/'.$node->nid, NULL, NULL, TRUE) ." ]\n\n";
        $body .= _notify_content($node, $user). "\n\n";
        //$body .= $mini_separator . "\n\n";
      }
    }

    // Write new comments to e-mail
    if ($user->comment && count($comments)) {
      $body .= $separator . "\n"
        . t('Recent comments - %count', array('%count' => format_plural(count($comments), '1 new comment', '%count new comments'))) ."\n"
        . $separator ."\n\n";
      
      $nid_old = 0;
      foreach ($comments as $nid => $comment) {
        if ($nid != $nid_old) {
          // If we don't already have the node, fetch it.
          if (!array_key_exists($nid, $nodes)) {
            $nodes[$nid] = node_load(array('nid' => $nid));
            $temp = node_invoke($node, 'node_info');
            $node->type_name = $temp[$node->type]['name'];
            $nodes[$node->nid] = $node;
          }
          $body .= t('%count new comments attached to %type posted by author: %title', 
            array('%count' => count($comment), '%title' => $nodes[$nid]->title, 
              '%type' => $node->type_name, '%author' => 
              ($node->name ? $node->name : variable_get('anonymous', 'Anonymous'))))."\n";

          $nid_old = $nid;
        }
        
        $comment_count = 0;
        foreach ($comment as $c) {
          $body .= '  '. t('%title by %author', array('%title' => $c->subject, '%author' => ($c->name ? $c->name : variable_get(anonymous, 'Anonymous')))) ."\n"
                .  '    '. url('node/'.$nid, NULL, 'comment-'.$c->cid, TRUE) ."\n\n";
        }
        //$body .= $mini_separator . "\n\n";
      }
    }

    // If there was anything new, send mail
    if ($body) {
      $body = t('Greetings %user,', array("%user" => $user->name))."\n\n$body";

      $body .= "\n-- \n";
      $body .= t('This is an automatic mail from %sitename', array('%sitename' => variable_get('site_name', 'drupal')))."\n";
      $body .= t('To stop receiving these mails go to %notify-url', array('%notify-url' => url("user/$user->uid/notify" , NULL, NULL, TRUE)))."\n";

      if (!user_mail($user->mail, $subject, wordwrap($body, 72), "From: $from_name <$from>\nX-Mailer: Drupal Notify module (http://drupal.org/project/notify)\nReturn-path: <$from>\nErrors-to: $from\n")) {
        $ret['failed']++;
        db_query('UPDATE {notify} SET attempts = attempts + 1 WHERE uid = %d', $user->uid);
        watchdog('error', t('Notify: User %name (%mail) could not be notified. Mail error.', array('%name' => '<em>'. $user->name .'</em>', '%mail' => $user->mail)));
      }
      else {
        $ret['sent']++;
        watchdog('user', t('Notify: User %name (%mail) notified successfully.', array('%name' => '<em>'. $user->name .'</em>', '%mail' => $user->mail)));
      }
    }
  }
  _notify_switch_user(); // Restore user
  return $ret;
}

function notify_entities_to_utf8($text) {
  static $table;
  // We store named entities in a table for quick processing.  
  if (!isset($table)) {
    // Get all named HTML entities.
    $table = array_flip(get_html_translation_table(HTML_ENTITIES));
    // PHP gives us Windows-1252/ISO-8859-1 data, we need UTF-8.
    $table = array_map('utf8_encode', $table);
  }
  $text = strtr($text, $table);

  // Any remaining entities are numerical. Use a regexp to replace them.
  return preg_replace('/&#(x?)([A-Za-z0-9]+);/e', '_notify_entity_to_utf8("$1", "$2")', $text);
}

function _notify_entity_to_utf8($hex, $codepoint) {
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

/**
 * Switch from original user to mail submision user and back.
 *
 * NOTE: Copied from mailhandler
 *
 * Note: You first need to run _notify_switch_user without
 * argument to store the current user. Call _notify_switch_user
 * without argument to set the user back to the original user.
 *
 * @param $uid The user ID to switch to
 *
 */
function _notify_switch_user($uid = NULL) {
  global $user;
  static $orig_user = array();

  if (isset($uid)) {
    $user = user_load(array('uid' => $uid));
  }
  // retrieve the initial user, can be called multiple times
  else if (count($orig_user)) {
    $user = array_shift($orig_user);
    array_unshift($orig_user, $user);
  }
  // store the initial user
  else {
    $orig_user[] = $user;
  }
}

