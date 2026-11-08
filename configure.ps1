$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
$form=New-Object Windows.Forms.Form
$form.Text='Memory Radio - connect Gemini';$form.Width=570;$form.Height=255;$form.StartPosition='CenterScreen'
$form.TopMost=$true;$form.ShowInTaskbar=$true
$label=New-Object Windows.Forms.Label
$label.Text='Enter your Gemini API key. Memory Radio sends recordings to Google when you choose Organize or submit a follow-up answer. Your key is encrypted on this Windows account.'
$label.SetBounds(18,18,515,60)
$box=New-Object Windows.Forms.TextBox;$box.UseSystemPasswordChar=$true;$box.SetBounds(18,87,515,27)
$button=New-Object Windows.Forms.Button;$button.Text='Save key';$button.SetBounds(18,136,200,35)
$button.Add_Click({if($box.Text.Trim().Length -gt 10){$form.DialogResult=[Windows.Forms.DialogResult]::OK;$form.Close()}})
$form.Controls.AddRange(@($label,$box,$button));$form.AcceptButton=$button
$form.Add_Shown({$form.Activate();$box.Focus()})
if($form.ShowDialog() -eq [Windows.Forms.DialogResult]::OK){
  ConvertTo-SecureString $box.Text.Trim() -AsPlainText -Force | Export-Clixml -LiteralPath (Join-Path $PSScriptRoot '.gemini-key.xml')
}
$box.Clear();$form.Dispose()
