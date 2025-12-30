#!/usr/bin/env python3
"""
Questionnaire Response Processor
Processes completed PROJECT_GOALS_REALIGNMENT_PLAN.md and generates structured data
for plan generation.
"""

import re
import json
from pathlib import Path
from typing import Dict, List, Any
from datetime import datetime

class QuestionnaireProcessor:
    def __init__(self, questionnaire_path: str):
        self.questionnaire_path = Path(questionnaire_path)
        self.responses = {}
        self.priorities = {}
        
    def parse_questionnaire(self) -> Dict[str, Any]:
        """Parse the questionnaire markdown file and extract responses."""
        if not self.questionnaire_path.exists():
            raise FileNotFoundError(f"Questionnaire not found: {self.questionnaire_path}")
        
        content = self.questionnaire_path.read_text(encoding='utf-8')
        
        # Extract project vision
        self.responses['project_vision'] = self._extract_project_vision(content)
        
        # Extract feature priorities
        self.responses['feature_priorities'] = self._extract_feature_priorities(content)
        
        # Extract technical requirements
        self.responses['technical_requirements'] = self._extract_technical_requirements(content)
        
        # Extract agent strategy
        self.responses['agent_strategy'] = self._extract_agent_strategy(content)
        
        # Extract quality gates
        self.responses['quality_gates'] = self._extract_quality_gates(content)
        
        # Extract timeline
        self.responses['timeline'] = self._extract_timeline(content)
        
        return self.responses
    
    def _extract_project_vision(self, content: str) -> Dict[str, Any]:
        """Extract project vision questions."""
        vision = {}
        
        # Primary goal
        goal_match = re.search(r'1\. \*\*What is the primary goal.*?\n(.*?)(?=\n\n|\n2\.)', content, re.DOTALL)
        if goal_match:
            vision['primary_goal'] = self._extract_checkbox_selection(goal_match.group(1))
        
        # Target timeline
        timeline_match = re.search(r'2\. \*\*What is the target timeline.*?\n(.*?)(?=\n\n|\n3\.)', content, re.DOTALL)
        if timeline_match:
            vision['target_timeline'] = self._extract_checkbox_selection(timeline_match.group(1))
        
        # Target audience
        audience_match = re.search(r'3\. \*\*What is the target audience.*?\n(.*?)(?=\n\n|##)', content, re.DOTALL)
        if audience_match:
            vision['target_audience'] = self._extract_checkbox_selection(audience_match.group(1))
        
        return vision
    
    def _extract_feature_priorities(self, content: str) -> Dict[str, Any]:
        """Extract feature priority rankings."""
        priorities = {}
        
        # Simulation features
        sim_section = self._extract_section(content, 'Simulation Features')
        if sim_section:
            priorities['simulation'] = self._parse_feature_section(sim_section)
        
        # Rendering features
        render_section = self._extract_section(content, 'Rendering Features')
        if render_section:
            priorities['rendering'] = self._parse_feature_section(render_section)
        
        # Performance features
        perf_section = self._extract_section(content, 'Performance Features')
        if perf_section:
            priorities['performance'] = self._parse_feature_section(perf_section)
        
        return priorities
    
    def _extract_technical_requirements(self, content: str) -> Dict[str, Any]:
        """Extract technical requirements."""
        requirements = {}
        
        # Code quality
        quality_section = self._extract_section(content, 'Code Quality Standards')
        if quality_section:
            requirements['code_quality'] = self._parse_requirements_section(quality_section)
        
        # Platform support
        platform_section = self._extract_section(content, 'Platform Support')
        if platform_section:
            requirements['platforms'] = self._parse_platform_section(platform_section)
        
        # Build system
        build_section = self._extract_section(content, 'Build System')
        if build_section:
            requirements['build'] = self._parse_build_section(build_section)
        
        return requirements
    
    def _extract_agent_strategy(self, content: str) -> Dict[str, Any]:
        """Extract agent development strategy."""
        strategy = {}
        
        # Autonomy level
        autonomy_section = self._extract_section(content, 'Agent Autonomy Level')
        if autonomy_section:
            strategy['autonomy'] = self._parse_autonomy_section(autonomy_section)
        
        # Communication
        comm_section = self._extract_section(content, 'Agent Communication')
        if comm_section:
            strategy['communication'] = self._parse_communication_section(comm_section)
        
        # Quality gates
        qg_section = self._extract_section(content, 'Quality Gates')
        if qg_section:
            strategy['quality_gates'] = self._parse_quality_gates_section(qg_section)
        
        return strategy
    
    def _extract_quality_gates(self, content: str) -> Dict[str, Any]:
        """Extract quality gate specifications."""
        gates = {}
        
        # Technical metrics
        tech_section = self._extract_section(content, 'Technical Metrics')
        if tech_section:
            gates['technical'] = self._parse_metrics_section(tech_section)
        
        # Process metrics
        process_section = self._extract_section(content, 'Process Metrics')
        if process_section:
            gates['process'] = self._parse_metrics_section(process_section)
        
        return gates
    
    def _extract_timeline(self, content: str) -> Dict[str, Any]:
        """Extract timeline information."""
        timeline = {}
        
        # Look for timeline mentions
        timeline_match = re.search(r'target timeline[:\s]+(.*?)(?:\n|$)', content, re.IGNORECASE)
        if timeline_match:
            timeline['target'] = timeline_match.group(1).strip()
        
        return timeline
    
    def _extract_section(self, content: str, section_title: str) -> str:
        """Extract a section from the content."""
        pattern = f'### {re.escape(section_title)}.*?(?=###|##|$)'
        match = re.search(pattern, content, re.DOTALL | re.IGNORECASE)
        return match.group(0) if match else ""
    
    def _extract_checkbox_selection(self, text: str) -> str:
        """Extract selected checkbox option."""
        # Look for checked checkboxes [x]
        checked = re.findall(r'- \[x\]\s*(.+?)(?:\n|$)', text, re.IGNORECASE)
        if checked:
            return checked[0].strip()
        
        # Look for filled blanks
        filled = re.findall(r':\s*([^\n]+)', text)
        if filled:
            return filled[0].strip()
        
        return "Not specified"
    
    def _parse_feature_section(self, section: str) -> Dict[str, Any]:
        """Parse a feature section."""
        features = {}
        
        # Extract feature blocks
        feature_blocks = re.findall(r'- \[.*?\] \*\*(.*?)\*\*.*?\n(.*?)(?=\n- \[|\n\n|$)', section, re.DOTALL)
        
        for feature_name, feature_content in feature_blocks:
            feature_name = feature_name.strip()
            features[feature_name] = {
                'priority': self._extract_field(feature_content, 'Priority'),
                'details': self._extract_details(feature_content)
            }
        
        return features
    
    def _parse_requirements_section(self, section: str) -> Dict[str, Any]:
        """Parse a requirements section."""
        requirements = {}
        
        # Extract numbered items
        items = re.findall(r'\d+\.\s*\*\*(.*?)\*\*.*?\n(.*?)(?=\d+\.|$)', section, re.DOTALL)
        
        for item_name, item_content in items:
            item_name = item_name.strip().lower().replace(' ', '_')
            requirements[item_name] = self._extract_field_values(item_content)
        
        return requirements
    
    def _parse_platform_section(self, section: str) -> Dict[str, Any]:
        """Parse platform support section."""
        platforms = {}
        
        platform_blocks = re.findall(r'- \[.*?\] \*\*(.*?)\*\*.*?\n(.*?)(?=\n- \[|\n\n|$)', section, re.DOTALL)
        
        for platform_name, platform_content in platform_blocks:
            platform_name = platform_name.strip()
            platforms[platform_name] = {
                'priority': self._extract_field(platform_content, 'Priority'),
                'details': self._extract_details(platform_content)
            }
        
        return platforms
    
    def _parse_build_section(self, section: str) -> Dict[str, Any]:
        """Parse build system section."""
        build = {}
        
        # Extract preferences
        items = re.findall(r'\d+\.\s*\*\*(.*?)\*\*.*?\n(.*?)(?=\d+\.|$)', section, re.DOTALL)
        
        for item_name, item_content in items:
            item_name = item_name.strip().lower().replace(' ', '_')
            build[item_name] = self._extract_checkbox_selection(item_content)
        
        return build
    
    def _parse_autonomy_section(self, section: str) -> Dict[str, Any]:
        """Parse agent autonomy section."""
        autonomy = {}
        
        items = re.findall(r'\d+\.\s*\*\*(.*?)\*\*.*?\n(.*?)(?=\d+\.|$)', section, re.DOTALL)
        
        for item_name, item_content in items:
            item_name = item_name.strip().lower().replace(' ', '_')
            autonomy[item_name] = self._extract_checkbox_selection(item_content)
        
        return autonomy
    
    def _parse_communication_section(self, section: str) -> Dict[str, Any]:
        """Parse communication section."""
        communication = {}
        
        items = re.findall(r'\d+\.\s*\*\*(.*?)\*\*.*?\n(.*?)(?=\d+\.|$)', section, re.DOTALL)
        
        for item_name, item_content in items:
            item_name = item_name.strip().lower().replace(' ', '_')
            communication[item_name] = self._extract_checkbox_selection(item_content)
        
        return communication
    
    def _parse_quality_gates_section(self, section: str) -> Dict[str, Any]:
        """Parse quality gates section."""
        gates = {}
        
        items = re.findall(r'\d+\.\s*\*\*(.*?)\*\*.*?\n(.*?)(?=\d+\.|$)', section, re.DOTALL)
        
        for item_name, item_content in items:
            item_name = item_name.strip().lower().replace(' ', '_')
            gates[item_name] = self._extract_checkbox_selection(item_content)
        
        return gates
    
    def _parse_metrics_section(self, section: str) -> Dict[str, Any]:
        """Parse metrics section."""
        metrics = {}
        
        items = re.findall(r'\d+\.\s*\*\*(.*?)\*\*.*?\n(.*?)(?=\d+\.|$)', section, re.DOTALL)
        
        for item_name, item_content in items:
            item_name = item_name.strip().lower().replace(' ', '_')
            metrics[item_name] = self._extract_field_values(item_content)
        
        return metrics
    
    def _extract_field(self, text: str, field_name: str) -> str:
        """Extract a field value."""
        pattern = f'{re.escape(field_name)}:\\s*([^\n]+)'
        match = re.search(pattern, text, re.IGNORECASE)
        return match.group(1).strip() if match else ""
    
    def _extract_field_values(self, text: str) -> Dict[str, str]:
        """Extract multiple field values."""
        values = {}
        lines = text.split('\n')
        
        for line in lines:
            if ':' in line and not line.strip().startswith('-'):
                parts = line.split(':', 1)
                if len(parts) == 2:
                    key = parts[0].strip().replace(' ', '_').lower()
                    value = parts[1].strip()
                    if value:
                        values[key] = value
        
        return values
    
    def _extract_details(self, text: str) -> Dict[str, Any]:
        """Extract detailed information from feature content."""
        details = {}
        
        # Extract checkboxes
        checkboxes = re.findall(r'- \[([ x])\]\s*(.+?)(?:\n|$)', text)
        for checked, label in checkboxes:
            key = label.strip().lower().replace(' ', '_')
            details[key] = checked.lower() == 'x'
        
        # Extract field values
        field_values = self._extract_field_values(text)
        details.update(field_values)
        
        return details
    
    def generate_summary(self) -> Dict[str, Any]:
        """Generate a summary of responses."""
        summary = {
            'processed_date': datetime.now().isoformat(),
            'questionnaire_path': str(self.questionnaire_path),
            'responses': self.responses,
            'summary': {
                'project_vision': self.responses.get('project_vision', {}),
                'feature_count': sum(len(f.get('features', {})) for f in self.responses.get('feature_priorities', {}).values()),
                'technical_requirements_count': len(self.responses.get('technical_requirements', {})),
                'agent_strategy_defined': bool(self.responses.get('agent_strategy')),
                'quality_gates_defined': bool(self.responses.get('quality_gates'))
            }
        }
        
        return summary
    
    def save_results(self, output_path: str):
        """Save processed results to JSON file."""
        summary = self.generate_summary()
        
        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(summary, f, indent=2, ensure_ascii=False)
        
        print(f"Results saved to: {output_file}")


def main():
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(description='Process questionnaire responses')
    parser.add_argument('questionnaire', help='Path to PROJECT_GOALS_REALIGNMENT_PLAN.md')
    parser.add_argument('-o', '--output', default='questionnaire_responses.json',
                       help='Output JSON file path')
    
    args = parser.parse_args()
    
    processor = QuestionnaireProcessor(args.questionnaire)
    responses = processor.parse_questionnaire()
    processor.save_results(args.output)
    
    print("\nQuestionnaire processing complete!")
    print(f"Extracted {len(responses)} response categories")


if __name__ == '__main__':
    main()

