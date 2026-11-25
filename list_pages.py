import json
j=json.load(open(r'C:\Users\liyizhen\.codex\skills\GordenPPTSkill\templates\architecture-deck\detail.json',encoding='utf-8'))
for p in j['pages']:
 print(p['slide_number'],p['use_for'],len(p['text_slots']),[(s['current_text'],s['role']) for s in p['text_slots'] if s['role'] in ('页面主标题','页面标题')][:2])
